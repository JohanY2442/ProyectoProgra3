#ifndef MOVIE_ITERATOR_H
#define MOVIE_ITERATOR_H

#include <vector>
#include <cstddef>

using namespace std;

// Patrón Iterator (con Template): pagina resultados de 5 en 5
template <typename T = int>
class MovieIterator {
private:
    vector<T> items_;
    size_t currentIndex_ = 0;
    size_t pageSize_ = 5;

public:
    MovieIterator() = default;
    explicit MovieIterator(const vector<T>& items, size_t pageSize = 5);

    bool hasNextPage() const;
    bool hasPrevPage() const;
    vector<T> getCurrentPage() const;

    void nextPage();
    void prevPage();
    void reset();

    size_t getCurrentPageNumber() const;
    size_t getTotalPages() const;
    size_t getTotalResults() const { return items_.size(); }
};

#endif
