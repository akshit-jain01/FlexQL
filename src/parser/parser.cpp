#include "../../include/parser/parser.h"
#include <sstream>
#include <algorithm>
#include<iostream>

using namespace std;

// ---------- helper ----------
vector<string> split(const string &s) {
    vector<string> tokens;
    stringstream ss(s);
    string word;
    while (ss >> word) tokens.push_back(word);
    return tokens;
}

// ---------- main parser ----------
Query parse_query(const string& query_str) {
    string query = query_str;

// trim leading spaces/newlines
    query.erase(0, query.find_first_not_of(" \n\r\t"));
    query.erase(query.find_last_not_of(" \n\r\t") + 1);
    Query q;
    auto tokens = split(query);

    if (tokens.empty()) {
        q.type = UNKNOWN;
        return q;
    }
    
    // ================= CREATE =================
    if (tokens[0] == "CREATE") {
        q.type = CREATE;

        // FIX: remove "(...)"
        string table_token;

        // handle IF NOT EXISTS
        if (tokens.size() >= 6 && tokens[2] == "IF") {
            table_token = tokens[5];  // CREATE TABLE IF NOT EXISTS X
        } else {
            table_token = tokens[2];  // normal CREATE
        }

    
        // remove "(..." if attached
        size_t pos = table_token.find("(");
        if (pos != string::npos)
            table_token = table_token.substr(0, pos);

        // 🔥 TRIM (apply ALWAYS, not just one branch)
        table_token.erase(0, table_token.find_first_not_of(" \n\r\t"));
        table_token.erase(table_token.find_last_not_of(" \n\r\t") + 1);

        q.table_name = table_token;
        cout << "TABLE: [" << q.table_name << "]\n";
        // extract columns
        size_t start = query_str.find("(");
        size_t end = query_str.rfind(")");
        if (start == string::npos || end == string::npos || end <= start) {
            q.type = UNKNOWN;
            return q;
        }

        string cols = query_str.substr(start + 1, end - start - 1);

        stringstream ss(cols);
        string col;

        while (getline(ss, col, ',')) {

            col.erase(0, col.find_first_not_of(" "));
            col.erase(col.find_last_not_of(" ") + 1);

            stringstream col_ss(col);
            string col_name, col_type;

            col_ss >> col_name >> col_type;

            transform(col_type.begin(), col_type.end(), col_type.begin(), ::toupper);

            q.columns.push_back(col_name);
            q.types.push_back(col_type);
        }
    }

    // ================= INSERT =================
    
    else if (tokens.size() > 0 && tokens[0] == "INSERT"){
        // std::cout << "DEBUG QUERY: " << query << endl;
        q.type = INSERT;
        q.table_name = tokens[2];

        size_t pos = query_str.find("VALUES");
        if (pos == string::npos) {
            q.type = UNKNOWN;
            return q;
        }

        string values_part = query_str.substr(pos + 6);
        // normalize
        replace(values_part.begin(), values_part.end(), '\n', ' ');
        replace(values_part.begin(), values_part.end(), '\r', ' ');

        // remove semicolon
        values_part.erase(remove(values_part.begin(), values_part.end(), ';'), values_part.end());

        vector<string> rows;
        string curr;
        int depth = 0;
        bool in_string=false;

        for (char c : values_part) {
            if (c == '\'') {
                in_string = !in_string;
            }
            if(!in_string){
                if (c == '(') {
                    depth++;
                    if (depth == 1) {
                        curr.clear();
                        continue;
                    }
                }

                if (c == ')') {
                    depth--;
                    if (depth == 0) {
                        rows.push_back(curr);
                        continue;
                    }
                }
            }

            if (depth >= 1) {
                curr += c;
            }
        }

        // 🔥 Parse each row (VERY IMPORTANT)
        
        for (auto &row : rows) {
            
            string val;

            // stringstream ss(row);

            // while (getline(ss, val, ',')) {
            //     val.erase(0, val.find_first_not_of(" "));
            //     val.erase(val.find_last_not_of(" ") + 1);
            //     vals.push_back(val);
            // }
            vector<string> vals;
            string curr;
            bool in_string = false;

            for (int i = 0; i < row.size(); i++) {
                char c = row[i];

                if (c == '\'') {
                    in_string = !in_string;
                    curr += c;
                }
                else if (c == ',' && !in_string) {
                    // trim
                    curr.erase(0, curr.find_first_not_of(" "));
                    curr.erase(curr.find_last_not_of(" ") + 1);

                    vals.push_back(curr);
                    curr.clear();
                }
                else {
                    curr += c;
                }
            }

            // last value
            if (!curr.empty()) {
                curr.erase(0, curr.find_first_not_of(" "));
                curr.erase(curr.find_last_not_of(" ") + 1);
                vals.push_back(curr);
            }

            if (!vals.empty())
                q.multi_values.push_back(vals);
        }

        // 🔥 safety check
        if (q.multi_values.empty()) {
            q.type = UNKNOWN;
        }
    }

    // ================= SELECT =================
    else if (tokens[0] == "SELECT") {

        q.type = SELECT;

        // extract columns
        size_t from_pos = query_str.find("FROM");
        string cols_part = query_str.substr(7, from_pos - 7);

        cols_part.erase(0, cols_part.find_first_not_of(" "));
        cols_part.erase(cols_part.find_last_not_of(" ") + 1);

        stringstream ss(cols_part);
        string col;

        while (getline(ss, col, ',')) {
            col.erase(0, col.find_first_not_of(" "));
            col.erase(col.find_last_not_of(" ") + 1);
            q.select_columns.push_back(col);
        }

        // q.table_name = tokens[3];
        for (int i = 0; i < tokens.size(); i++) {
            if (tokens[i] == "FROM" && i + 1 < tokens.size()) {
                q.table_name = tokens[i + 1];
                break;
            }
        }
        // 🔥 remove ';'
        if (!q.table_name.empty() && q.table_name.back() == ';') {
            q.table_name.pop_back();
        }

        // 🔥 trim
        q.table_name.erase(0, q.table_name.find_first_not_of(" \n\r\t"));
        q.table_name.erase(q.table_name.find_last_not_of(" \n\r\t") + 1);

        // 🔥 normalize
        transform(q.table_name.begin(), q.table_name.end(), q.table_name.begin(), ::toupper);
        cout << "SELECT table lookup: [" << q.table_name << "] len=" << q.table_name.size() << "\n";


        // 🔥 REMOVE semicolon
        if (!q.table_name.empty() && q.table_name.back() == ';') {
            q.table_name.pop_back();
        }

        // 🔥 TRIM
        q.table_name.erase(0, q.table_name.find_first_not_of(" \n\r\t"));
        q.table_name.erase(q.table_name.find_last_not_of(" \n\r\t") + 1);

        // 🔥 NORMALIZE
        transform(q.table_name.begin(), q.table_name.end(), q.table_name.begin(), ::toupper);
        
        // ================= JOIN =================
        for (int i = 0; i < tokens.size(); i++) {
            if (tokens[i] == "JOIN") {
                q.is_join = true;
                q.join_table = tokens[i + 1];

                // 🔥 REMOVE semicolon
                if (!q.join_table.empty() && q.join_table.back() == ';') {
                    q.join_table.pop_back();
                }

                // 🔥 TRIM
                q.join_table.erase(0, q.join_table.find_first_not_of(" \n\r\t"));
                q.join_table.erase(q.join_table.find_last_not_of(" \n\r\t") + 1);

                // 🔥 NORMALIZE
                transform(q.join_table.begin(), q.join_table.end(), q.join_table.begin(), ::toupper);

                string left = tokens[i + 3];
                string right = tokens[i + 5];

                q.join_col1 = left.substr(left.find('.') + 1);
                q.join_col2 = right.substr(right.find('.') + 1);

                q.join_col1.erase(0, q.join_col1.find_first_not_of(" "));
                q.join_col1.erase(q.join_col1.find_last_not_of(" ") + 1);

                q.join_col2.erase(0, q.join_col2.find_first_not_of(" "));
                q.join_col2.erase(q.join_col2.find_last_not_of(" ") + 1);
            }
        }

        // ================= WHERE =================
        for (int i = 0; i < tokens.size(); i++) {
            if (tokens[i] == "WHERE") {
            
                q.where_column = tokens[i + 1];
                q.where_op = tokens[i + 2];
                q.where_value = tokens[i + 3];
                size_t dot = q.where_column.find('.');
                if (dot != string::npos) {
                    q.where_column = q.where_column.substr(dot + 1);
                }
                if (!q.where_value.empty() && q.where_value.back() == ';') {
                    q.where_value.pop_back();
                }
            }

        }
        // ================= ORDER BY =================
        for (int i = 0; i < tokens.size(); i++) {
            if (tokens[i] == "ORDER" && i + 2 < tokens.size()) {

                // ORDER BY column
                q.order_by_column = tokens[i + 2];

                // remove trailing ';' if present
                if (!q.order_by_column.empty() && q.order_by_column.back() == ';') {
                    q.order_by_column.pop_back();
                }

                // DESC or ASC
                if (i + 3 < tokens.size()) {
                    string ord = tokens[i + 3];

                    // remove ';' if present
                    if (!ord.empty() && ord.back() == ';') {
                        ord.pop_back();
                    }

                    if (ord == "DESC") {
                        q.order_desc = true;
                    }
                }
            }
        }
    }

    // ================= DELETE =================
    else if (tokens[0] == "DELETE") {
        q.type = DELETE;
        

        
        // for (auto &p : database) {
        //     cout << "[" << p.first << "]\n";
        // }
        
        q.table_name = tokens[2];

        // 🔥 REMOVE SEMICOLON
        if (!q.table_name.empty() && q.table_name.back() == ';') {
            q.table_name.pop_back();
        }

        // 🔥 TRIM
        q.table_name.erase(0, q.table_name.find_first_not_of(" \n\r\t"));
        q.table_name.erase(q.table_name.find_last_not_of(" \n\r\t") + 1);

        // 🔥 OPTIONAL (recommended)
        transform(q.table_name.begin(), q.table_name.end(), q.table_name.begin(), ::toupper);
        cout << "DELETE looking for: [" << q.table_name << "]\n";
    }
    else {
        q.type = UNKNOWN;
    }

    return q;
}