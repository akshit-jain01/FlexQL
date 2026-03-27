#pragma once
#include "query.h"
#include <string>


std::string execute_query_obj(const Query& q, const std::string& raw_query);
    
// In executor.h, add:
bool check_cache(const std::string& raw_query, std::string& result);