#ifndef USER_OBSERVER_H
#define USER_OBSERVER_H

#include "Movie.h"
#include <vector>
#include <string>

using namespace std;

// Patrón Observer: interfaz para recibir cambios del usuario
class IUserObserver {
public:
    virtual ~IUserObserver() = default;
    virtual void actualizar(const vector<int>& likedMovies, const vector<int>& watchLaterMovies) = 0;
};

// Patrón Observer: sujeto que administra Likes y Ver más tarde
class UserProfileSubject {
private:
    vector<IUserObserver*> observadores_;
    vector<int> likedMovies_;
    vector<int> watchLaterMovies_;

public:
    void agregarObservador(IUserObserver* obs);
    void notificarObservadores();

    bool addLike(int movieId);
    bool addWatchLater(int movieId);
    void setInitialData(const vector<int>& likes, const vector<int>& watchLater);

    const vector<int>& getLikedMovies() const { return likedMovies_; }
    const vector<int>& getWatchLaterMovies() const { return watchLaterMovies_; }
};

// Observador que calcula películas similares según los Likes
class RecommendationObserver : public IUserObserver {
private:
    const vector<Movie>& allMovies_;
    vector<int> recommendedMovieIds_;

public:
    explicit RecommendationObserver(const vector<Movie>& allMovies);
    void actualizar(const vector<int>& likedMovies, const vector<int>& watchLaterMovies) override;
    const vector<int>& getRecommendations() const { return recommendedMovieIds_; }
};

// Observador que guarda y carga las listas del usuario en disco
class PersistenceObserver : public IUserObserver {
private:
    string filename_;

public:
    explicit PersistenceObserver(const string& filename = "user_data.txt");
    void actualizar(const vector<int>& likedMovies, const vector<int>& watchLaterMovies) override;
    void loadFromDisk(vector<int>& likedMovies, vector<int>& watchLaterMovies);
};

#endif
