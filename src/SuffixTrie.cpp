#include "SuffixTrie.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <chrono>

using namespace std;

// Functor para ordenar películas de mayor a menor importancia
struct MovieRankComparator {
    bool operator()(const pair<int, int>& a, const pair<int, int>& b) const {
        if (a.second != b.second) return a.second > b.second;
        return a.first < b.first;
    }
};

SuffixTrie::SuffixTrie() {
    for (int i = 0; i < NUM_SHARDS; ++i) {
        roots_[i] = new TrieNode();
    }
    // Conectores comunes excluidos en sinopsis para optimizar RAM
    stopWords_ = {"the", "and", "for", "that", "with", "his", "her", "from", "are", "was"};
}

SuffixTrie::~SuffixTrie() {
    // Libera la memoria de los 4 sub-árboles en paralelo
    vector<thread> threads;
    for (int i = 0; i < NUM_SHARDS; ++i) {
        threads.emplace_back(&SuffixTrie::deleteNode, this, roots_[i]);
    }
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }
}

// Libera recursivamente los nodos en PostOrden
void SuffixTrie::deleteNode(TrieNode* node) {
    if (!node) return;
    for (auto& par : node->children) {
        deleteNode(par.second);
    }
    delete node;
}

string SuffixTrie::toLower(const string& text) {
    string result = text;
    transform(result.begin(), result.end(), result.begin(),
              [](unsigned char c) { return static_cast<char>(tolower(c)); });
    return result;
}

// Separa un texto en palabras alfanuméricas individuales
vector<string> SuffixTrie::tokenizeWords(const string& text) {
    vector<string> words;
    string current;
    for (unsigned char c : text) {
        if (isalnum(c)) {
            current += static_cast<char>(tolower(c));
        } else if (!current.empty()) {
            words.push_back(current);
            current.clear();
        }
    }
    if (!current.empty()) words.push_back(current);
    return words;
}

// Inserta un sufijo carácter por carácter en el sub-árbol
void SuffixTrie::insertSuffix(TrieNode* root, const string& suffix, int movieIndex, bool isTitle) {
    TrieNode* current = root;
    for (char c : suffix) {
        auto it = current->children.find(c);
        if (it == current->children.end()) {
            TrieNode* newNode = new TrieNode();
            current->children[c] = newNode;
            current = newNode;
        } else {
            current = it->second;
        }

        if (isTitle) {
            if (current->titleMovieIds.empty() || current->titleMovieIds.back() != movieIndex)
                current->titleMovieIds.push_back(movieIndex);
        } else {
            if (current->plotMovieIds.empty() || current->plotMovieIds.back() != movieIndex)
                current->plotMovieIds.push_back(movieIndex);
        }
    }
}

// Extrae e inserta todos los sufijos de cada palabra del texto
void SuffixTrie::insertTextWords(TrieNode* root, const string& text, int movieIndex, bool isTitle) {
    vector<string> words = tokenizeWords(text);
    for (const string& word : words) {
        if (!isTitle && (word.length() <= 2 || stopWords_.find(word) != stopWords_.end()))
            continue;

        for (size_t i = 0; i < word.length(); ++i) {
            insertSuffix(root, word.substr(i), movieIndex, isTitle);
        }
    }
}

// Carga el CSV y construye los sub-árboles usando hilos en paralelo
bool SuffixTrie::loadMoviesParallel(const string& filename) {
    ifstream file(filename);
    if (!file.is_open()) return false;

    auto startTime = chrono::high_resolution_clock::now();

    string record;
    readCSVRecord(file, record);

    vector<string> rawRecords;
    while (readCSVRecord(file, record)) {
        if (!record.empty()) rawRecords.push_back(std::move(record));
    }
    file.close();

    size_t totalRecords = rawRecords.size();
    movies_.resize(totalRecords);

    vector<thread> workers;
    size_t chunkSize = (totalRecords + NUM_SHARDS - 1) / NUM_SHARDS;

    // Cada hilo procesa un cuarto de las películas sin bloqueo de mutex
    for (int shard = 0; shard < NUM_SHARDS; ++shard) {
        size_t startIdx = shard * chunkSize;
        size_t endIdx = min(startIdx + chunkSize, totalRecords);

        workers.emplace_back([this, shard, startIdx, endIdx, &rawRecords]() {
            for (size_t i = startIdx; i < endIdx; ++i) {
                movies_[i] = parseCSVLine(rawRecords[i], static_cast<int>(i));
                insertTextWords(roots_[shard], movies_[i].title, static_cast<int>(i), true);
                insertTextWords(roots_[shard], movies_[i].plot, static_cast<int>(i), false);
            }
        });
    }

    for (auto& t : workers) {
        if (t.joinable()) t.join();
    }

    auto endTime = chrono::high_resolution_clock::now();
    double elapsed = chrono::duration<double>(endTime - startTime).count();

    lock_guard<mutex> lock(mtx_);
    cout << "✓ Cargadas " << movies_.size() << " peliculas en " << elapsed << " s.\n";
    return true;
}

// Busca una palabra o sub-palabra y asigna puntaje de importancia
void SuffixTrie::searchWordInShard(int shardIdx, const string& word,
                                   unordered_map<int, int>& localScores) const {
    TrieNode* current = roots_[shardIdx];
    for (char c : word) {
        auto it = current->children.find(c);
        if (it == current->children.end()) return;
        current = it->second;
    }

    // Título vale +10 puntos; Sinopsis vale +3 puntos
    for (int id : current->titleMovieIds) localScores[id] += 10;
    for (int id : current->plotMovieIds) localScores[id] += 3;
}

// Busca palabras, frases o sub-palabras en paralelo usando async y future
vector<int> SuffixTrie::searchByText(const string& query) {
    vector<string> words = tokenizeWords(query);
    if (words.empty()) return {};

    vector<future<unordered_map<int, int>>> futures;

    for (int shard = 0; shard < NUM_SHARDS; ++shard) {
        futures.push_back(async(launch::async, [this, shard, &words]() {
            unordered_map<int, int> shardScores;
            for (const string& w : words) {
                unordered_map<int, int> wordHits;
                searchWordInShard(shard, w, wordHits);
                for (const auto& pair : wordHits) {
                    shardScores[pair.first] += pair.second + 5;
                }
            }
            return shardScores;
        }));
    }

    unordered_map<int, int> totalScores;
    for (auto& f : futures) {
        unordered_map<int, int> partial = f.get();
        for (const auto& pair : partial) {
            totalScores[pair.first] += pair.second;
        }
    }

    // Bono extra si la frase completa aparece seguida en el título
    string cleanQuery = toLower(query);
    for (auto& pair : totalScores) {
        if (movies_[pair.first].title.find(cleanQuery) != string::npos) {
            pair.second += 25;
        }
    }

    vector<pair<int, int>> rankedList(totalScores.begin(), totalScores.end());
    sort(rankedList.begin(), rankedList.end(), MovieRankComparator());

    vector<int> results;
    results.reserve(rankedList.size());
    for (const auto& item : rankedList) results.push_back(item.first);
    return results;
}

// Busca por tag en paralelo usando promise, future y thread
vector<int> SuffixTrie::searchByTag(const string& tagType, const string& value) {
    string lowerVal = toLower(value);
    if (lowerVal.empty()) return {};

    vector<promise<vector<int>>> promises(NUM_SHARDS);
    vector<future<vector<int>>> futures(NUM_SHARDS);
    vector<thread> threads;

    size_t total = movies_.size();
    size_t chunkSize = (total + NUM_SHARDS - 1) / NUM_SHARDS;

    for (int i = 0; i < NUM_SHARDS; ++i) {
        futures[i] = promises[i].get_future();
        size_t startIdx = i * chunkSize;
        size_t endIdx = min(startIdx + chunkSize, total);

        threads.emplace_back([this, startIdx, endIdx, &tagType, &lowerVal](promise<vector<int>>* prom) {
            vector<int> localMatches;
            for (size_t idx = startIdx; idx < endIdx; ++idx) {
                const Movie& m = movies_[idx];
                bool match = false;
                if (tagType == "director" && m.director.find(lowerVal) != string::npos) match = true;
                else if (tagType == "genre" && m.genre.find(lowerVal) != string::npos) match = true;
                else if (tagType == "cast" && m.cast.find(lowerVal) != string::npos) match = true;
                else if (tagType == "origin" && m.origin.find(lowerVal) != string::npos) match = true;

                if (match) localMatches.push_back(static_cast<int>(idx));
            }
            prom->set_value(std::move(localMatches));
        }, &promises[i]);
    }

    vector<int> finalResults;
    for (int i = 0; i < NUM_SHARDS; ++i) {
        vector<int> part = futures[i].get();
        finalResults.insert(finalResults.end(), part.begin(), part.end());
    }
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }
    return finalResults;
}

const Movie& SuffixTrie::getMovieByIndex(int idx) const {
    static Movie emptyMovie;
    if (idx >= 0 && idx < static_cast<int>(movies_.size())) return movies_[idx];
    return emptyMovie;
}

int SuffixTrie::getTotalMovies() const {
    return static_cast<int>(movies_.size());
}

// Lee un registro completo respetando saltos de línea entre comillas
bool SuffixTrie::readCSVRecord(istream& in, string& record) {
    string line;
    if (!getline(in, record)) return false;
    while (count(record.begin(), record.end(), '"') % 2 != 0 && getline(in, line)) {
        record += " " + line;
    }
    if (!record.empty() && record.back() == '\r') record.pop_back();
    return true;
}

// Parsea una fila del CSV y crea la película usando Patrón Builder
Movie SuffixTrie::parseCSVLine(const string& line, int movieId) {
    vector<string> fields;
    string field;
    bool inQuotes = false;

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (c == '"') {
            if (inQuotes && i + 1 < line.size() && line[i + 1] == '"') {
                field += '"';
                i++;
            } else {
                inQuotes = !inQuotes;
            }
        } else if (c == ',' && !inQuotes) {
            fields.push_back(field);
            field.clear();
        } else {
            field += c;
        }
    }
    fields.push_back(field);

    int year = 0;
    try { year = fields.size() > 0 ? stoi(fields[0]) : 0; } catch (...) { year = 0; }

    return MovieBuilder()
        .setId(movieId)
        .setYear(year)
        .setTitle(fields.size() > 1 ? toLower(fields[1]) : "")
        .setOrigin(fields.size() > 2 ? toLower(fields[2]) : "")
        .setDirector(fields.size() > 3 ? toLower(fields[3]) : "")
        .setCast(fields.size() > 4 ? toLower(fields[4]) : "")
        .setGenre(fields.size() > 5 ? toLower(fields[5]) : "")
        .setWikiUrl(fields.size() > 6 ? fields[6] : "")
        .setPlot(fields.size() > 7 ? toLower(fields[7]) : "")
        .build();
}
