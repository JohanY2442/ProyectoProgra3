#ifndef SUFFIX_TRIE_H
#define SUFFIX_TRIE_H

#include "Movie.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <set>
#include <mutex>
#include <thread>
#include <future>
#include <istream>

using namespace std;

// Interfaz común de búsqueda para aplicar el Patrón Proxy
class ISearchEngine {
public:
    virtual ~ISearchEngine() = default;
    virtual vector<int> searchByText(const string& query) = 0;
    virtual vector<int> searchByTag(const string& tagType, const string& value) = 0;
};

// Nodo del Trie que almacena hijos e índices de películas
struct TrieNode {
    unordered_map<char, TrieNode*> children;
    vector<int> titleMovieIds;
    vector<int> plotMovieIds;
};

// Árbol de sufijos particionado para inserción y búsqueda paralela
class SuffixTrie : public ISearchEngine {
private:
    static const int NUM_SHARDS = 4;
    TrieNode* roots_[NUM_SHARDS];
    vector<Movie> movies_;
    set<string> stopWords_;
    mutex mtx_;

    void deleteNode(TrieNode* node);
    void insertSuffix(TrieNode* root, const string& suffix, int movieIndex, bool isTitle);
    void insertTextWords(TrieNode* root, const string& text, int movieIndex, bool isTitle);
    void searchWordInShard(int shardIdx, const string& word, unordered_map<int, int>& localScores) const;

    bool readCSVRecord(istream& in, string& record);
    Movie parseCSVLine(const string& line, int movieId);

public:
    SuffixTrie();
    ~SuffixTrie() override;

    SuffixTrie(const SuffixTrie&) = delete;
    SuffixTrie& operator=(const SuffixTrie&) = delete;

    bool loadMoviesParallel(const string& filename);
    vector<int> searchByText(const string& query) override;
    vector<int> searchByTag(const string& tagType, const string& value) override;

    const Movie& getMovieByIndex(int idx) const;
    int getTotalMovies() const;
    const vector<Movie>& getAllMovies() const { return movies_; }

    static string toLower(const string& text);
    static vector<string> tokenizeWords(const string& text);
};

#endif
