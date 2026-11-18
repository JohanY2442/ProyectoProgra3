#include "MovieIterator.h"
#include <algorithm>

using namespace std;

template <typename T>
MovieIterator<T>::MovieIterator(const vector<T>& items, size_t pageSize)
    : items_(items), currentIndex_(0), pageSize_(pageSize) {}

template <typename T>
bool MovieIterator<T>::hasNextPage() const {
    return currentIndex_ + pageSize_ < items_.size();
}

template <typename T>
bool MovieIterator<T>::hasPrevPage() const {
    return currentIndex_ >= pageSize_;
}

// Obtiene el subconjunto de elementos de la página actual
template <typename T>
vector<T> MovieIterator<T>::getCurrentPage() const {
    vector<T> page;
    size_t endIdx = min(currentIndex_ + pageSize_, items_.size());
    for (size_t i = currentIndex_; i < endIdx; ++i) {
        page.push_back(items_[i]);
    }
    return page;
}

template <typename T>
void MovieIterator<T>::nextPage() {
    if (hasNextPage()) currentIndex_ += pageSize_;
}

template <typename T>
void MovieIterator<T>::prevPage() {
    if (hasPrevPage()) currentIndex_ -= pageSize_;
}

template <typename T>
void MovieIterator<T>::reset() {
    currentIndex_ = 0;
}

template <typename T>
size_t MovieIterator<T>::getCurrentPageNumber() const {
    if (items_.empty()) return 0;
    return (currentIndex_ / pageSize_) + 1;
}

template <typename T>
size_t MovieIterator<T>::getTotalPages() const {
    if (items_.empty()) return 0;
    return (items_.size() + pageSize_ - 1) / pageSize_;
}

// Instanciación explícita del template para enteros
template class MovieIterator<int>;
