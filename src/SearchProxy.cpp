#include "SearchProxy.h"

using namespace std;

SearchCacheProxy::SearchCacheProxy(ISearchEngine* realEngine)
    : realEngine_(realEngine) {}

// Patrón Proxy: verifica primero en caché antes de consultar el árbol
vector<int> SearchCacheProxy::searchByText(const string& query) {
    string key = SuffixTrie::toLower(query);
    {
        lock_guard<mutex> lock(cacheMutex_);
        auto it = textCache_.find(key);
        if (it != textCache_.end()) return it->second;
    }

    vector<int> results = realEngine_->searchByText(query);
    {
        lock_guard<mutex> lock(cacheMutex_);
        textCache_[key] = results;
    }
    return results;
}

vector<int> SearchCacheProxy::searchByTag(const string& tagType, const string& value) {
    string key = SuffixTrie::toLower(tagType + ":" + value);
    {
        lock_guard<mutex> lock(cacheMutex_);
        auto it = tagCache_.find(key);
        if (it != tagCache_.end()) return it->second;
    }

    vector<int> results = realEngine_->searchByTag(tagType, value);
    {
        lock_guard<mutex> lock(cacheMutex_);
        tagCache_[key] = results;
    }
    return results;
}

void SearchCacheProxy::clearCache() {
    lock_guard<mutex> lock(cacheMutex_);
    textCache_.clear();
    tagCache_.clear();
}
