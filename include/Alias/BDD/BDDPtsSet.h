/*
 * BDD-backed points-to set using the CUDD package.
 *
 * This header intentionally hides all CUDD types behind a pimpl to avoid
 * leaking the heavy dependency into most translation units. The actual
 * implementation lives in lib/Alias/BDD/BDDPtsSet.cpp.
 */

#ifndef ANDERSEN_BDDPTSSET_H
#define ANDERSEN_BDDPTSSET_H

#include <cstdint>
#include <memory>
#include <vector>

class BDDAndersPtsSet {
public:
  using Index = std::uint64_t;
  using iterator = std::vector<Index>::const_iterator;

  BDDAndersPtsSet();
  BDDAndersPtsSet(const BDDAndersPtsSet &);
  BDDAndersPtsSet(BDDAndersPtsSet &&) noexcept;
  BDDAndersPtsSet &operator=(const BDDAndersPtsSet &);
  BDDAndersPtsSet &operator=(BDDAndersPtsSet &&) noexcept;
  ~BDDAndersPtsSet();

  bool has(Index idx);
  bool has(Index idx) const;
  bool insert(Index idx);
  bool contains(const BDDAndersPtsSet &other) const;
  bool intersectWith(const BDDAndersPtsSet &other) const;
  bool unionWith(const BDDAndersPtsSet &other);

  void clear();
  unsigned getSize() const;
  bool isEmpty() const;
  bool operator==(const BDDAndersPtsSet &other) const;

  iterator begin() const;
  iterator end() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl;

  // Materialize the BDD into a stable snapshot for iteration.
  void refreshCache() const;
  mutable std::shared_ptr<std::vector<Index>> cache;
};

#endif // ANDERSEN_BDDPTSSET_H
