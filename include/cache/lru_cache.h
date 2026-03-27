#pragma once
#include <unordered_map>
#include <list>
#include <string>

class LRUCache {
private:
    int capacity;

    // key → (value, iterator in list)
    std::unordered_map<std::string, 
        std::pair<std::string, std::list<std::string>::iterator>> cache;

    std::list<std::string> lru; // most recent at front

public:
    LRUCache(int cap);

    bool get(const std::string& key, std::string& value);

    void put(const std::string& key, const std::string& value);
};