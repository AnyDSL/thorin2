#pragma once

#include <ranges>

#include "mim/util/vector.h"

namespace mim {

template<class Indexer, class Key, class Value> class IndexMap {
private:
    template<class T> struct IsValidPred {
        static bool is_valid(T) { return true; }
    };

    template<class T> struct IsValidPred<T*> {
        static bool is_valid(T* value) { return value != nullptr; }
    };

public:
    IndexMap(const IndexMap& other)
        : indexer_(other.indexer_)
        , array_(other.array_) {}
    IndexMap(IndexMap&& other) noexcept
        : indexer_(std::move(other.indexer_))
        , array_(std::move(other.array_)) {}
    IndexMap(const Indexer& indexer, const Value& value = Value())
        : indexer_(indexer)
        , array_(indexer.size(), value) {}
    IndexMap(const Indexer& indexer, View<Value> array)
        : indexer_(indexer)
        , array_(array) {}
    template<class I>
    IndexMap(const Indexer& indexer, const I begin, const I end)
        : indexer_(indexer)
        , array_(begin, end) {}
    IndexMap& operator=(IndexMap other) noexcept { return swap(*this, other), *this; }

    const Indexer& indexer() const { return indexer_; }
    size_t capacity() const { return array_.size(); }
    Value& operator[](Key key) {
        auto i = indexer().index(key);
        assert(i != size_t(-1));
        return array_[i];
    }
    const Value& operator[](Key key) const { return const_cast<IndexMap*>(this)->operator[](key); }
    auto& array() { return array_; }
    const auto& array() const { return array_; }
    Value& array(size_t i) { return array_[i]; }
    const Value& array(size_t i) const { return array_[i]; }

    auto begin() const { return std::views::filter(array_, IsValidPred<Value>::is_valid).begin(); }
    auto end() const { return std::views::filter(array_, IsValidPred<Value>::is_valid).end(); }

    friend void swap(IndexMap& map1, IndexMap& map2) noexcept {
        using std::swap;
        swap(map1.indexer_, map2.indexer_);
        swap(map1.array_, map2.array_);
    }

private:
    const Indexer& indexer_;
    Vector<Value> array_;
};

/// @name IndexMap find
///@{
template<class Indexer, class Key, class Value> inline Value* find(IndexMap<Indexer, Key, Value*>& map, Key key) {
    auto i = map.indexer().index(key);
    return i != size_t(-1) ? map.array()[i] : nullptr;
}

template<class Indexer, class Key, class Value>
inline const Value* find(const IndexMap<Indexer, Key, Value*>& map, Key key) {
    return find(const_cast<IndexMap<Indexer, Key, Value*>&>(map), key);
}
///@}

} // namespace mim
