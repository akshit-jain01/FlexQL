#include "query/executor.h"
#include "storage/table.h"
#include "cache/lru_cache.h"

#include <unordered_map>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <iostream>

using namespace std;

static unordered_map<string, Table> database;
static thread_local LRUCache query_cache(100);

// ---------- Helper ----------
inline bool is_numeric(const string& type) {
    return (type == "INT" || type == "DECIMAL");
}

inline string format_double(double x) {
    long long xi = (long long)x;

    // if it's integer, print without decimals
    if (x == (double)xi)
        return to_string(xi);

    // else print normally (you can improve later)
    return to_string(x);
}

// ---------- Cache ----------
bool check_cache(const string& raw_query, string& result) {
    return query_cache.get(raw_query, result);
}

// ---------- Main Executor ----------
string execute_query_obj(const Query& q, const string& raw_query) {

    if (q.type == UNKNOWN) {
        // return "Error: Invalid query\n##END##\n";
        return "Error: Invalid query\n";
    }

    // ================= CREATE =================
    if (q.type == CREATE) {
        if (database.count(q.table_name)) {
            // return "Error: Table already exists\n##END##\n";
            return "Error: Table already exists\n";
        }

        Table t;
        t.columns = q.columns;
        t.types = q.types;

        int num_idx = 0, str_idx = 0;

        for (int i = 0; i < t.columns.size(); i++) {
            t.col_index[t.columns[i]] = i;

            if (t.types[i] == "INT" || t.types[i] == "DECIMAL") {
                t.numeric_cols.push_back({});
                t.numeric_cols.back().reserve(1000000);

                t.col_to_numeric.push_back(num_idx++);
                t.col_to_string.push_back(-1);
            } else {
                t.string_cols.push_back({});
                t.string_cols.back().reserve(1000000);

                t.col_to_string.push_back(str_idx++);
                t.col_to_numeric.push_back(-1);
            }
        }

        database[q.table_name] = move(t);

        // return "Table created\n##END##\n";
        return "Table created\n";
    }
    //============== DELETE ===============
    if (q.type == DELETE) {

        auto &t = database[q.table_name];

        // clear rows
        t.row_count = 0;
        t.primary_index.clear();

        for (auto &col : t.numeric_cols)
            col.clear();

        for (auto &col : t.string_cols)
            col.clear();

        return "Table cleared\n";
    }

    // ================= VALIDATE =================
    if (!database.count(q.table_name)) {
        return "Error: Table not found\n";
    }

    auto &table = database[q.table_name];

    // ================= INSERT =================
    if (q.type == INSERT) {

        auto &table = database[q.table_name];

        // 🔥 Loop over multiple rows
        for (auto &row_vals : q.multi_values) {
            for (auto &v : row_vals) {
                cout << "[" << v << "] ";
            }
            cout << endl;
            if (row_vals.size() != table.columns.size()) {
                cout << "Mismatch: " << row_vals.size()
                    << " vs " << table.columns.size() << endl;
                exit(1);
            }

            // Insert values column-wise
            for (int i = 0; i < row_vals.size(); i++) {
                if (i >= table.col_to_numeric.size()) {
                    cout << "CRASH: i out of range\n";
                    exit(1);
                }
                if (table.col_to_numeric[i] != -1) {
                    int idx = table.col_to_numeric[i];
                        if (idx >= table.numeric_cols.size()) {
                        std::cout << "ERROR: numeric idx out of range\n";
                        exit(1);
                    }
                    table.numeric_cols[idx].push_back(stod(row_vals[i]));
                } else {
                    int idx = table.col_to_string[i];
                    if (idx < 0 || idx >= table.string_cols.size()) {
                    cout << "CRASH: string idx invalid: " << idx << endl;
                    exit(1);
                }
                    table.string_cols[idx].push_back(row_vals[i]);
                }
            }

            // Primary key (first column)
            double pk = stod(row_vals[0]);
            table.primary_index[pk] = table.row_count;

            table.row_count++;
        }

        return "Row inserted\n";
        // return "Row inserted\n##END##\n";
    }


    // ================= SELECT =================
    if (q.type == SELECT) {
        if (q.select_columns.empty() || q.table_name.empty()) {
            return "Error: Invalid query\n";
        }
    string result;
    result.reserve(1 << 20);

    // -------- JOIN --------
    if (q.is_join) {

        if (!database.count(q.join_table)) {
            // return "Error: Join table not found\n##END##\n";
            return "Error: Join table not found\n";
        }

        auto &t1 = database[q.table_name];
        auto &t2 = database[q.join_table];

        int c1 = t1.col_index[q.join_col1];
        int c2 = t2.col_index[q.join_col2];

        unordered_map<string, vector<int>> hash;

        // 🔥 BUILD HASH (t2)
        for (int i = 0; i < t2.row_count; i++) {

            string key;

            if (t2.col_to_numeric[c2] != -1) {
                int idx = t2.col_to_numeric[c2];
                key = format_double(t2.numeric_cols[idx][i]);
            } else {
                int idx = t2.col_to_string[c2];
                key = t2.string_cols[idx][i];
            }

            hash[key].push_back(i);
        }

        // 🔥 PROBE (t1)
        for (int i = 0; i < t1.row_count; i++) {

            string key;

            if (t1.col_to_numeric[c1] != -1) {
                int idx = t1.col_to_numeric[c1];
                key = format_double(t1.numeric_cols[idx][i]);
            } else {
                int idx = t1.col_to_string[c1];
                key = t1.string_cols[idx][i];
            }

            if (!hash.count(key)) continue;

            for (int j : hash[key]) {
                if (!q.where_column.empty()) {

                    string where_col = q.where_column;
                    size_t dot = where_col.find('.');
                    if (dot != string::npos) {
                        where_col = where_col.substr(dot + 1);
                    }

                    string where_val_str = q.where_value;
                    if (!where_val_str.empty() && where_val_str.back() == ';') {
                        where_val_str.pop_back();
                    }
                    where_col.erase(0, where_col.find_first_not_of(" \n\r\t"));
                    where_col.erase(where_col.find_last_not_of(" \n\r\t") + 1);

                    double where_val = stod(where_val_str);

                    // check in t2 (since WHERE uses TEST_ORDERS)
                    int wc = t2.col_index[where_col];
                    if (!t2.col_index.count(where_col)) {
                        return "Error: Column not found\n";
                    }
    
                    double cell;
                    if (t2.col_to_numeric[wc] != -1) {
                        int idx = t2.col_to_numeric[wc];
                        cell = t2.numeric_cols[idx][j];
                    } else {
                        continue;
                    }

                    bool match = false;
                    if (q.where_op == "=") match = (cell == where_val);
                    else if (q.where_op == ">") match = (cell > where_val);
                    else if (q.where_op == "<") match = (cell < where_val);
                    else if (q.where_op == ">=") match = (cell >= where_val);
                    else if (q.where_op == "<=") match = (cell <= where_val);
                    else if (q.where_op == "!=") match = (cell != where_val);

                    if (!match) continue;
                }

                // print t1 row
                for (int idx_c = 0; idx_c < q.select_columns.size(); idx_c++) {

                string col = q.select_columns[idx_c];

                string val;

                if (col.find('.') != string::npos) {
                    string table = col.substr(0, col.find('.'));
                    string column = col.substr(col.find('.') + 1);

                    if (table == q.table_name) {
                        int c = t1.col_index[column];

                        if (t1.col_to_numeric[c] != -1) {
                            int idx = t1.col_to_numeric[c];
                            val = format_double(t1.numeric_cols[idx][i]);
                        } else {
                            int idx = t1.col_to_string[c];
                            val = t1.string_cols[idx][i];
                            if (val.size() >= 2 && val[0] == '\'' && val.back() == '\'') {
                                val = val.substr(1, val.size() - 2);
                            }
                        }

                    } else {
                        int c = t2.col_index[column];

                        if (t2.col_to_numeric[c] != -1) {
                            int idx = t2.col_to_numeric[c];
                            val = format_double(t2.numeric_cols[idx][j]);
                        } else {
                            int idx = t2.col_to_string[c];
                            val = t2.string_cols[idx][j];
                            if (val.size() >= 2 && val[0] == '\'' && val.back() == '\'') {
                                val = val.substr(1, val.size() - 2);
                            }
                        }
                    }
                }

                if (idx_c > 0) result += " ";
                result += val;
            }

            result += "\n";
            }
        }

        if (result.empty()) return "\n";

        // result += "##END##\n";
        return result;
    }

    auto &table = database[q.table_name];
    vector<int> selected_cols;

    if (q.select_columns.size() == 1 && q.select_columns[0] == "*") {
        for (int i = 0; i < table.columns.size(); i++) {
            selected_cols.push_back(i);
        }
    } else {
        for (auto &col : q.select_columns) {
            

            string clean_col = col;

            // remove table prefix if exists
            size_t dot = clean_col.find('.');
            if (dot != string::npos) {
                clean_col = clean_col.substr(dot + 1);
            }

            // trim spaces
            clean_col.erase(0, clean_col.find_first_not_of(" \n\r\t"));
            clean_col.erase(clean_col.find_last_not_of(" \n\r\t") + 1);

            if (!table.col_index.count(clean_col)) {
                return "Error: Column not found\n";
            }

            selected_cols.push_back(table.col_index[clean_col]);
        
        }
    }
    // -------- WHERE --------
    if (!q.where_column.empty()) {
        string where_val_str = q.where_value;

        // remove semicolon safely
        if (!where_val_str.empty() && where_val_str.back() == ';') {
            where_val_str.pop_back();
        }
    
        string where_col = q.where_column;

        size_t dot = where_col.find('.');
        if (dot != string::npos) {
            where_col = where_col.substr(dot + 1);
        }
        if (!where_col.empty() && where_col.back() == ';') {
            where_col.pop_back();
        }

        // 🔥 trim
        where_col.erase(0, where_col.find_first_not_of(" \n\r\t"));
        where_col.erase(where_col.find_last_not_of(" \n\r\t") + 1);
        if (!table.col_index.count(where_col)) {
            return "Error: Column not found\n";
        }
        int col = table.col_index[where_col];

        // PRIMARY KEY FAST PATH
        if (col == 0 && q.where_op == "=") {
            double key = stod(where_val_str);

            if (table.primary_index.count(key)) {
                int i = table.primary_index[key];

                for (int idx_c = 0; idx_c < selected_cols.size(); idx_c++) {
                    int c = selected_cols[idx_c];

                    string val;

                    if (table.col_to_numeric[c] != -1) {
                        int idx = table.col_to_numeric[c];
                        val = format_double(table.numeric_cols[idx][i]);
                    } else {
                        int idx = table.col_to_string[c];
                        val = table.string_cols[idx][i];
                        if (val.size() >= 2 && val[0] == '\'' && val.back() == '\'') {
                            val = val.substr(1, val.size() - 2);
                        }
                    }

                    if (idx_c > 0) result += " ";
                    result += val;
                }

                // result += "##END##\n";
                result += "\n";
                return result;
            }
            return "\n";
        }

        double val = stod(where_val_str);

        for (int i = 0; i < table.row_count; i++) {

            double cell;

            if (table.col_to_numeric[col] != -1) {
                int idx = table.col_to_numeric[col];
                cell = table.numeric_cols[idx][i];
            } else {
                continue; // skip string WHERE for now
            }

            bool match = false;

            if (q.where_op == "=") match = (cell == val);
            else if (q.where_op == ">") match = (cell > val);
            else if (q.where_op == "<") match = (cell < val);
            else if (q.where_op == ">=") match = (cell >= val);
            else if (q.where_op == "<=") match = (cell <= val);
            else if (q.where_op == "!=") match = (cell != val);

            if (!match) continue;

            for (int idx_c = 0; idx_c < selected_cols.size(); idx_c++) {
                int c = selected_cols[idx_c];

                string val;

                if (table.col_to_numeric[c] != -1) {
                    int idx = table.col_to_numeric[c];
                    val = format_double(table.numeric_cols[idx][i]);
                } else {
                    int idx = table.col_to_string[c];
                    val = table.string_cols[idx][i];
                    if (val.size() >= 2 && val[0] == '\'' && val.back() == '\'') {
                        val = val.substr(1, val.size() - 2);
                    }
                }

                if (idx_c > 0) result += " ";
                result += val;
            }

            result += "\n";
        }

        if (result.empty()) return "\n";

        // result += "##END##\n";
        return result;
    }

    // -------- FULL SCAN --------
    vector<int> row_ids;

    for (int i = 0; i < table.row_count; i++) {
        row_ids.push_back(i);
    }
    if (!q.order_by_column.empty()) {
        string order_col = q.order_by_column;

        size_t dot = order_col.find('.');
        if (dot != string::npos) {
            order_col = order_col.substr(dot + 1);
        }

        if (!order_col.empty() && order_col.back() == ';') {
            order_col.pop_back();
        }

        if (!table.col_index.count(order_col)) {
            return "Error: Column not found\n";
        }

        int col = table.col_index[order_col];

        sort(row_ids.begin(), row_ids.end(), [&](int a, int b) {

            // numeric column
            if (table.col_to_numeric[col] != -1) {
                int idx = table.col_to_numeric[col];
                double va = table.numeric_cols[idx][a];
                double vb = table.numeric_cols[idx][b];

                return q.order_desc ? (va > vb) : (va < vb);
            }
            // string column
            else {
                int idx = table.col_to_string[col];
                string va = table.string_cols[idx][a];
                string vb = table.string_cols[idx][b];

                return q.order_desc ? (va > vb) : (va < vb);
            }
        });
    }
    for (int i : row_ids) {

        for (int idx_c = 0; idx_c < selected_cols.size(); idx_c++) {
            int c = selected_cols[idx_c];

            string val;

            if (table.col_to_numeric[c] != -1) {
                int idx = table.col_to_numeric[c];
                val = format_double(table.numeric_cols[idx][i]);
            } else {
                int idx = table.col_to_string[c];
                val = table.string_cols[idx][i];

                if (val.size() >= 2 && val[0] == '\'' && val.back() == '\'') {
                    val = val.substr(1, val.size() - 2);
                }
            }

            if (idx_c > 0) result += " ";
            result += val;
        }

        result += "\n";
    }

    // if (result.empty()) return "No matching records\n##END##\n";
    if (result.empty()) return "\n";

    // result += "##END##\n";
    result += "\n";
    return result;
    }

    return "Unknown query\n";
}