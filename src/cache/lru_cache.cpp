#include "../../include/cache/lru_cache.h"

using namespace std;

LRUCache::LRUCache(int cap) {
    capacity = cap;
}

bool LRUCache::get(const string& key, string& value) {
    if (cache.find(key) == cache.end())
        return false;

    // Move to front (most recent)
    lru.erase(cache[key].second);
    lru.push_front(key);
    cache[key].second = lru.begin();

    value = cache[key].first;
    return true;
}

void LRUCache::put(const string& key, const string& value) {

    // If exists → update
    if (cache.find(key) != cache.end()) {
        lru.erase(cache[key].second);
    }

    // If full → remove LRU
    else if (lru.size() == capacity) {
        string lru_key = lru.back();
        lru.pop_back();
        cache.erase(lru_key);
    }

    // Insert new
    lru.push_front(key);
    cache[key] = {value, lru.begin()};
}