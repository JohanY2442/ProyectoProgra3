#ifndef MOVIE_H
#define MOVIE_H

#include <string>

using namespace std;

// Estructura de datos de una película
struct Movie {
    int id = -1;
    int year = 0;
    string title;
    string origin;
    string director;
    string cast;
    string genre;
    string wiki_url;
    string plot;
};

// Patrón Builder: construye objetos Movie paso a paso
class MovieBuilder {
private:
    Movie movie_;
public:
    MovieBuilder& setId(int id) { movie_.id = id; return *this; }
    MovieBuilder& setYear(int year) { movie_.year = year; return *this; }
    MovieBuilder& setTitle(const string& title) { movie_.title = title; return *this; }
    MovieBuilder& setOrigin(const string& origin) { movie_.origin = origin; return *this; }
    MovieBuilder& setDirector(const string& director) { movie_.director = director; return *this; }
    MovieBuilder& setCast(const string& cast) { movie_.cast = cast; return *this; }
    MovieBuilder& setGenre(const string& genre) { movie_.genre = genre; return *this; }
    MovieBuilder& setWikiUrl(const string& url) { movie_.wiki_url = url; return *this; }
    MovieBuilder& setPlot(const string& plot) { movie_.plot = plot; return *this; }

    Movie build() const { return movie_; }
};

#endif
