#include "StreamingPlatform.h"

using namespace std;

StreamingPlatform* StreamingPlatform::instance_ = nullptr;

StreamingPlatform::StreamingPlatform() {
    searchProxy_ = new SearchCacheProxy(&trieEngine_);
    recommender_ = new RecommendationObserver(trieEngine_.getAllMovies());
    persistence_ = new PersistenceObserver("user_data.txt");

    userProfile_.agregarObservador(recommender_);
    userProfile_.agregarObservador(persistence_);
}

StreamingPlatform::~StreamingPlatform() {
    delete searchProxy_;
    delete recommender_;
    delete persistence_;
}

// Patrón Singleton: devuelve la única instancia creada
StreamingPlatform* StreamingPlatform::getInstance() {
    if (instance_ == nullptr) {
        instance_ = new StreamingPlatform();
    }
    return instance_;
}

bool StreamingPlatform::initialize(const vector<string>& possibleCsvPaths) {
    bool loaded = false;
    for (const string& path : possibleCsvPaths) {
        if (trieEngine_.loadMoviesParallel(path)) {
            loaded = true;
            break;
        }
    }
    if (!loaded) return false;

    vector<int> savedLikes, savedWatchLater;
    persistence_->loadFromDisk(savedLikes, savedWatchLater);
    userProfile_.setInitialData(savedLikes, savedWatchLater);
    return true;
}

MovieIterator<int> StreamingPlatform::searchMovies(const string& query) {
    return MovieIterator<int>(searchProxy_->searchByText(query), 5);
}

MovieIterator<int> StreamingPlatform::searchByTag(const string& tagType, const string& value) {
    return MovieIterator<int>(searchProxy_->searchByTag(tagType, value), 5);
}

bool StreamingPlatform::likeMovie(int movieId) {
    return userProfile_.addLike(movieId);
}

bool StreamingPlatform::watchLaterMovie(int movieId) {
    return userProfile_.addWatchLater(movieId);
}

const Movie& StreamingPlatform::getMovie(int movieId) const {
    return trieEngine_.getMovieByIndex(movieId);
}

const vector<int>& StreamingPlatform::getWatchLaterList() const {
    return userProfile_.getWatchLaterMovies();
}

const vector<int>& StreamingPlatform::getLikedList() const {
    return userProfile_.getLikedMovies();
}

const vector<int>& StreamingPlatform::getRecommendedList() const {
    return recommender_->getRecommendations();
}

int StreamingPlatform::getTotalMovies() const {
    return trieEngine_.getTotalMovies();
}
