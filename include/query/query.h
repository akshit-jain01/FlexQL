#pragma once
#include <string>
#include <vector>

using namespace std;

enum QueryType {
    CREATE,
    INSERT,
    SELECT,
    DELETE,
    UNKNOWN
};

struct Query {
    QueryType type;
    bool is_join = false;

    string join_table;
    string join_col1;
    string join_col2;

    string table_name;

    // CREATE
    vector<std::string> columns;

    // INSERT
    vector<std::string> values;
    vector<std::string> types; 
    // SELECT
    string order_by_column = "";
    bool order_desc = false;
    vector<std::string> select_columns;
    vector<vector<string>> multi_values;
    string where_column;
    string where_value;
    string where_op;
};