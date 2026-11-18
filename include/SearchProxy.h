#ifndef SEARCH_PROXY_H
#define SEARCH_PROXY_H

#include "SuffixTrie.h"
#include <unordered_map>
#include <string>
#include <vector>
#include <mutex>

using namespace std;

// Patrón Proxy: guarda búsquedas en caché para responder en O(1)
class SearchCacheProxy : public ISearchEngine {
private:
    ISearchEngine* realEngine_;
    unordered_map<string, vector<int>> textCache_;
    unordered_map<string, vector<int>> tagCache_;
    mutex cacheMutex_;

public:
    explicit SearchCacheProxy(ISearchEngine* realEngine);
    ~SearchCacheProxy() override = default;

    vector<int> searchByText(const string& query) override;
    vector<int> searchByTag(const string& tagType, const string& value) override;
    void clearCache();
};

#endif
