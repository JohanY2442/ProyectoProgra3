#ifndef STREAMING_PLATFORM_H
#define STREAMING_PLATFORM_H

#include "SuffixTrie.h"
#include "SearchProxy.h"
#include "MovieIterator.h"
#include "UserObserver.h"
#include <string>
#include <vector>

using namespace std;

// Patrón Singleton: garantiza una única instancia de la plataforma
class StreamingPlatform {
private:
    static StreamingPlatform* instance_;

    SuffixTrie trieEngine_;
    SearchCacheProxy* searchProxy_;
    UserProfileSubject userProfile_;
    RecommendationObserver* recommender_;
    PersistenceObserver* persistence_;

    StreamingPlatform();

public:
    StreamingPlatform(const StreamingPlatform&) = delete;
    StreamingPlatform& operator=(const StreamingPlatform&) = delete;
    ~StreamingPlatform();

    static StreamingPlatform* getInstance();

    bool initialize(const vector<string>& possibleCsvPaths);
    MovieIterator<int> searchMovies(const string& query);
    MovieIterator<int> searchByTag(const string& tagType, const string& value);

    bool likeMovie(int movieId);
    bool watchLaterMovie(int movieId);
    bool unlikeMovie(int movieId);
    bool removeWatchLater(int movieId);

    const Movie& getMovie(int movieId) const;
    const vector<int>& getWatchLaterList() const;
    const vector<int>& getLikedList() const;
    const vector<int>& getRecommendedList() const;
    int getTotalMovies() const;
};

#endif
