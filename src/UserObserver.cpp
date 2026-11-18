#include "UserObserver.h"
#include "SuffixTrie.h"
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <sstream>

using namespace std;

void UserProfileSubject::agregarObservador(IUserObserver* obs) {
    observadores_.push_back(obs);
}

// Patrón Observer: notifica a todos los observadores registrados
void UserProfileSubject::notificarObservadores() {
    for (IUserObserver* obs : observadores_) {
        obs->actualizar(likedMovies_, watchLaterMovies_);
    }
}

bool UserProfileSubject::addLike(int movieId) {
    if (find(likedMovies_.begin(), likedMovies_.end(), movieId) != likedMovies_.end())
        return false;
    likedMovies_.push_back(movieId);
    notificarObservadores();
    return true;
}

bool UserProfileSubject::addWatchLater(int movieId) {
    if (find(watchLaterMovies_.begin(), watchLaterMovies_.end(), movieId) != watchLaterMovies_.end())
        return false;
    watchLaterMovies_.push_back(movieId);
    notificarObservadores();
    return true;
}

void UserProfileSubject::setInitialData(const vector<int>& likes, const vector<int>& watchLater) {
    likedMovies_ = likes;
    watchLaterMovies_ = watchLater;
    notificarObservadores();
}

RecommendationObserver::RecommendationObserver(const vector<Movie>& allMovies)
    : allMovies_(allMovies) {}

// Algoritmo que recomienda las 5 películas más similares a los Likes
void RecommendationObserver::actualizar(const vector<int>& likedMovies,
                                        const vector<int>& /*watchLaterMovies*/) {
    recommendedMovieIds_.clear();
    if (likedMovies.empty() || allMovies_.empty()) return;

    unordered_set<int> likedSet(likedMovies.begin(), likedMovies.end());
    unordered_map<string, int> favGenres;
    unordered_map<string, int> favDirectors;

    for (int id : likedMovies) {
        if (id < 0 || id >= static_cast<int>(allMovies_.size())) continue;
        const Movie& m = allMovies_[id];

        for (const string& g : SuffixTrie::tokenizeWords(m.genre)) {
            if (g.length() > 2) favGenres[g] += 3;
        }
        if (!m.director.empty() && m.director != "unknown") {
            favDirectors[m.director] += 5;
        }
    }

    vector<pair<int, int>> candidateScores;
    for (size_t i = 0; i < allMovies_.size(); ++i) {
        int id = static_cast<int>(i);
        if (likedSet.count(id)) continue;

        const Movie& m = allMovies_[i];
        int score = 0;

        if (favDirectors.count(m.director)) score += favDirectors[m.director];
        for (const string& g : SuffixTrie::tokenizeWords(m.genre)) {
            if (favGenres.count(g)) score += favGenres[g];
        }

        if (score > 0) candidateScores.push_back({id, score});
    }

    sort(candidateScores.begin(), candidateScores.end(),
         [](const pair<int, int>& a, const pair<int, int>& b) {
             if (a.second != b.second) return a.second > b.second;
             return a.first < b.first;
         });

    size_t limit = min(candidateScores.size(), size_t(5));
    for (size_t i = 0; i < limit; ++i) {
        recommendedMovieIds_.push_back(candidateScores[i].first);
    }
}

PersistenceObserver::PersistenceObserver(const string& filename)
    : filename_(filename) {}

// Guarda automáticamente en disco los Likes y Ver más tarde
void PersistenceObserver::actualizar(const vector<int>& likedMovies,
                                     const vector<int>& watchLaterMovies) {
    ofstream out(filename_);
    if (!out.is_open()) return;

    out << "LIKES:";
    for (int id : likedMovies) out << " " << id;
    out << "\nWATCH_LATER:";
    for (int id : watchLaterMovies) out << " " << id;
    out << "\n";
}

void PersistenceObserver::loadFromDisk(vector<int>& likedMovies, vector<int>& watchLaterMovies) {
    ifstream in(filename_);
    if (!in.is_open()) return;

    string line;
    while (getline(in, line)) {
        stringstream ss(line);
        string tag;
        ss >> tag;
        int id;
        if (tag == "LIKES:") {
            while (ss >> id) likedMovies.push_back(id);
        } else if (tag == "WATCH_LATER:") {
            while (ss >> id) watchLaterMovies.push_back(id);
        }
    }
}
