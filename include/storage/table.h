#pragma once
#include <vector>
#include <string>
#include <unordered_map>

using namespace std;

struct Table {
    vector<string> columns;
    vector<string> types;

    vector<vector<double>> numeric_cols;
    vector<vector<string>> string_cols;

    vector<int> col_to_numeric;
    vector<int> col_to_string;

    unordered_map<string, int> col_index;

    unordered_map<double, int> primary_index;

    int row_count = 0;
};