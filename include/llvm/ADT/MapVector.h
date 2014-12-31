//===- llvm/ADT/MapVector.h - Map w/ deterministic value order --*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements a map that provides insertion order iteration. The
// interface is purposefully minimal. The key is assumed to be cheap to copy
// and 2 copies are kept, one for indexing in a DenseMap, one for iteration in
// a std::vector.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ADT_MAPVECTOR_H
#define LLVM_ADT_MAPVECTOR_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include <vector>

namespace llvm {

/// This class implements a map that also provides access to all stored values
/// in a deterministic order. The values are kept in a std::vector and the
/// mapping is done with DenseMap from Keys to indexes in that vector.
template<typename KeyT, typename ValueT,
         typename MapType = llvm::DenseMap<KeyT, unsigned>,
         typename VectorType = std::vector<std::pair<KeyT, ValueT> > >
class MapVector {
  typedef typename VectorType::size_type size_type;

  MapType Map;
  VectorType Vector;

public:
  typedef typename VectorType::iterator iterator;
  typedef typename VectorType::const_iterator const_iterator;
  typedef typename VectorType::reverse_iterator reverse_iterator;
  typedef typename VectorType::const_reverse_iterator const_reverse_iterator;

  size_type size() const { return Vector.size(); }

  iterator begin() { return Vector.begin(); }
  const_iterator begin() const { return Vector.begin(); }
  iterator end() { return Vector.end(); }
  const_iterator end() const { return Vector.end(); }

  reverse_iterator rbegin() { return Vector.rbegin(); }
  const_reverse_iterator rbegin() const { return Vector.rbegin(); }
  reverse_iterator rend() { return Vector.rend(); }
  const_reverse_iterator rend() const { return Vector.rend(); }

  bool empty() const {
    return Vector.empty();
  }

  std::pair<KeyT, ValueT>       &front()       { return Vector.front(); }
  const std::pair<KeyT, ValueT> &front() const { return Vector.front(); }
  std::pair<KeyT, ValueT>       &back()        { return Vector.back(); }
  const std::pair<KeyT, ValueT> &back()  const { return Vector.back(); }

  void clear() {
    Map.clear();
    Vector.clear();
  }

  ValueT &operator[](const KeyT &Key) {
    std::pair<KeyT, unsigned> Pair = std::make_pair(Key, 0);
    std::pair<typename MapType::iterator, bool> Result = Map.insert(Pair);
    unsigned &I = Result.first->second;
    if (Result.second) {
      Vector.push_back(std::make_pair(Key, ValueT()));
      I = Vector.size() - 1;
    }
    return Vector[I].second;
  }

  ValueT lookup(const KeyT &Key) const {
    typename MapType::const_iterator Pos = Map.find(Key);
    return Pos == Map.end()? ValueT() : Vector[Pos->second].second;
  }

  std::pair<iterator, bool> insert(const std::pair<KeyT, ValueT> &KV) {
    std::pair<KeyT, unsigned> Pair = std::make_pair(KV.first, 0);
    std::pair<typename MapType::iterator, bool> Result = Map.insert(Pair);
    unsigned &I = Result.first->second;
    if (Result.second) {
      Vector.push_back(std::make_pair(KV.first, KV.second));
      I = Vector.size() - 1;
      return std::make_pair(std::prev(end()), true);
    }
    return std::make_pair(begin() + I, false);
  }

  size_type count(const KeyT &Key) const {
    typename MapType::const_iterator Pos = Map.find(Key);
    return Pos == Map.end()? 0 : 1;
  }

  iterator find(const KeyT &Key) {
    typename MapType::const_iterator Pos = Map.find(Key);
    return Pos == Map.end()? Vector.end() :
                            (Vector.begin() + Pos->second);
  }

  const_iterator find(const KeyT &Key) const {
    typename MapType::const_iterator Pos = Map.find(Key);
    return Pos == Map.end()? Vector.end() :
                            (Vector.begin() + Pos->second);
  }

  /// \brief Remove the last element from the vector.
  void pop_back() {
    typename MapType::iterator Pos = Map.find(Vector.back().first);
    Map.erase(Pos);
    Vector.pop_back();
  }

  /// \brief Remove the element given by Iterator.
  ///
  /// Returns an iterator to the element following the one which was removed,
  /// which may be end().
  ///
  /// \note This is a deceivingly expensive operation (linear time).  It's
  /// usually better to use \a remove_if() if possible.
  typename VectorType::iterator erase(typename VectorType::iterator Iterator) {
    Map.erase(Iterator->first);
    auto Next = Vector.erase(Iterator);
    if (Next == Vector.end())
      return Next;

    // Update indices in the map.
    size_t Index = Next - Vector.begin();
    for (auto &I : Map) {
      assert(I.second != Index && "Index was already erased!");
      if (I.second > Index)
        --I.second;
    }
    return Next;
  }

  /// \brief Remove all elements with the key value Key.
  ///
  /// Returns the number of elements removed.
  size_type erase(const KeyT &Key) {
    auto Iterator = find(Key);
    if (Iterator == end())
      return 0;
    erase(Iterator);
    return 1;
  }

  /// \brief Remove the elements that match the predicate.
  ///
  /// Erase all elements that match \c Pred in a single pass.  Takes linear
  /// time.
  template <class Predicate> void remove_if(Predicate Pred);
};

template <typename KeyT, typename ValueT, typename MapType, typename VectorType>
template <class Function>
void MapVector<KeyT, ValueT, MapType, VectorType>::remove_if(Function Pred) {
  auto O = Vector.begin();
  for (auto I = O, E = Vector.end(); I != E; ++I) {
    if (Pred(*I)) {
      // Erase from the map.
      Map.erase(I->first);
      continue;
    }

    if (I != O) {
      // Move the value and update the index in the map.
      *O = std::move(*I);
      Map[O->first] = O - Vector.begin();
    }
    ++O;
  }
  // Erase trailing entries in the vector.
  Vector.erase(O, Vector.end());
}

/// \brief A MapVector that performs no allocations if smaller than a certain
/// size.
template <typename KeyT, typename ValueT, unsigned N>
class SmallMapVector
    : public MapVector<KeyT, ValueT, SmallDenseMap<KeyT, unsigned, N>,
                       SmallVector<std::pair<KeyT, ValueT>, N>> {
public:
  SmallMapVector() {}
};

} // end namespace llvm

#endif
