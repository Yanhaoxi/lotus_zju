// A real BDD-backed points-to set built on CUDD.
#include "Alias/BDD/BDDPtsSet.h"

#include "Solvers/CUDD/cudd.h"

#include <cmath>
#include <mutex>
#include <vector>

namespace {

// Use a fixed-width binary encoding for element indices.
using Index = BDDAndersPtsSet::Index;
constexpr unsigned kIndexBits = sizeof(Index) * 8;

DdManager *getManager() {
  static DdManager *manager = nullptr;
  static std::once_flag initFlag;
  std::call_once(initFlag, []() {
    manager = Cudd_Init(kIndexBits, 0, CUDD_UNIQUE_SLOTS, CUDD_CACHE_SLOTS, 0);
    Cudd_AutodynDisable(manager);
  });
  return manager;
}

std::vector<DdNode *> &cubeCache() {
  static std::vector<DdNode *> cubes;
  return cubes;
}

// Build (and cache) a cube that encodes a specific element index.
DdNode *getCube(Index idx) {
  auto &cubes = cubeCache();
  if (idx < cubes.size() && cubes[idx])
    return cubes[idx];

  if (idx >= cubes.size())
    cubes.resize(idx + 1, nullptr);

  DdManager *mgr = getManager();
  DdNode *cube = Cudd_ReadOne(mgr);
  Cudd_Ref(cube);
  for (unsigned bit = 0; bit < kIndexBits; ++bit) {
    const Index mask = Index{1} << bit;
    DdNode *var = Cudd_bddIthVar(mgr, bit);
    DdNode *lit = (idx & mask) ? var : Cudd_Not(var); // NOLINT
    DdNode *tmp = Cudd_bddAnd(mgr, cube, lit);
    Cudd_Ref(tmp);
    Cudd_RecursiveDeref(mgr, cube);
    cube = tmp;
  }

  cubes[idx] = cube;
  return cube;
}

inline DdNode *logicZero() { return Cudd_ReadLogicZero(getManager()); }

} // namespace

struct BDDAndersPtsSet::Impl {
  explicit Impl(DdNode *root) : bdd(root) { Cudd_Ref(bdd); }
  Impl() : Impl(logicZero()) {}
  ~Impl() { Cudd_RecursiveDeref(getManager(), bdd); }

  DdNode *bdd;
};

BDDAndersPtsSet::BDDAndersPtsSet() : impl(new Impl()) {}

BDDAndersPtsSet::BDDAndersPtsSet(const BDDAndersPtsSet &other)
    : impl(new Impl(other.impl->bdd)) {}

BDDAndersPtsSet::BDDAndersPtsSet(BDDAndersPtsSet &&other) noexcept
    : impl(std::move(other.impl)), cache(std::move(other.cache)) {}

BDDAndersPtsSet &BDDAndersPtsSet::operator=(const BDDAndersPtsSet &other) {
  if (this == &other)
    return *this;
  impl = std::make_unique<Impl>(other.impl->bdd);
  cache.reset();
  return *this;
}

BDDAndersPtsSet &BDDAndersPtsSet::operator=(BDDAndersPtsSet &&other) noexcept {
  if (this == &other)
    return *this;
  impl = std::move(other.impl);
  cache = std::move(other.cache);
  return *this;
}

BDDAndersPtsSet::~BDDAndersPtsSet() = default;

bool BDDAndersPtsSet::has(Index idx) {
  return static_cast<const BDDAndersPtsSet &>(*this).has(idx);
}

bool BDDAndersPtsSet::has(Index idx) const {
  return Cudd_bddLeq(getManager(), getCube(idx), impl->bdd);
}

bool BDDAndersPtsSet::insert(Index idx) {
  DdManager *mgr = getManager();
  DdNode *cube = getCube(idx);
  if (Cudd_bddLeq(mgr, cube, impl->bdd))
    return false;

  DdNode *merged = Cudd_bddOr(mgr, impl->bdd, cube);
  Cudd_Ref(merged);
  Cudd_RecursiveDeref(mgr, impl->bdd);
  impl->bdd = merged;
  cache.reset();
  return true;
}

bool BDDAndersPtsSet::contains(const BDDAndersPtsSet &other) const {
  return Cudd_bddLeq(getManager(), other.impl->bdd, impl->bdd);
}

bool BDDAndersPtsSet::intersectWith(const BDDAndersPtsSet &other) const {
  DdManager *mgr = getManager();
  DdNode *intersection = Cudd_bddAnd(mgr, impl->bdd, other.impl->bdd);
  Cudd_Ref(intersection);
  const bool nonEmpty = intersection != logicZero();
  Cudd_RecursiveDeref(mgr, intersection);
  return nonEmpty;
}

bool BDDAndersPtsSet::unionWith(const BDDAndersPtsSet &other) {
  DdManager *mgr = getManager();
  DdNode *merged = Cudd_bddOr(mgr, impl->bdd, other.impl->bdd);
  Cudd_Ref(merged);
  const bool changed = merged != impl->bdd;
  Cudd_RecursiveDeref(mgr, impl->bdd);
  impl->bdd = merged;
  if (changed)
    cache.reset();
  return changed;
}

void BDDAndersPtsSet::clear() {
  DdManager *mgr = getManager();
  if (impl->bdd == logicZero())
    return;
  Cudd_RecursiveDeref(mgr, impl->bdd);
  impl->bdd = logicZero();
  Cudd_Ref(impl->bdd);
  cache.reset();
}

unsigned BDDAndersPtsSet::getSize() const {
  const double count = Cudd_CountMinterm(getManager(), impl->bdd, kIndexBits);
  return static_cast<unsigned>(std::lround(count));
}

bool BDDAndersPtsSet::isEmpty() const { return impl->bdd == logicZero(); }

bool BDDAndersPtsSet::operator==(const BDDAndersPtsSet &other) const {
  return impl->bdd == other.impl->bdd;
}

void BDDAndersPtsSet::refreshCache() const {
  if (cache)
    return;

  auto elems = std::make_shared<std::vector<Index>>();
  if (isEmpty()) {
    cache = elems;
    return;
  }

  DdGen *gen;
  int *cube;
  CUDD_VALUE_TYPE value;
  Cudd_ForeachCube(getManager(), impl->bdd, gen, cube, value) {
    // Build a base index from bits fixed to 0/1 and record positions of
    // "don't-care" bits (encoded as 2 in CUDD).
    Index base = 0;
    unsigned dcCount = 0;
    unsigned dcPositions[kIndexBits]; // kIndexBits is small (≤64), stack-allocate.

    for (unsigned bit = 0; bit < kIndexBits; ++bit) {
      if (cube[bit] == 0) {
        // fixed to 0 – nothing to do
      } else if (cube[bit] == 1) {
        base |= (Index{1} << bit);
      } else { // cube[bit] == 2  → don't-care
        dcPositions[dcCount++] = bit;
      }
    }

    const Index combos = Index{1} << dcCount;
    for (Index m = 0; m < combos; ++m) {
      Index idx = base;
      for (unsigned i = 0; i < dcCount; ++i) {
        if (m & (Index{1} << i))
          idx |= (Index{1} << dcPositions[i]);
        else
          idx &= ~(Index{1} << dcPositions[i]); // ensure 0 when bit not set
      }
      elems->push_back(idx);
    }
  }

  cache = elems;
}

BDDAndersPtsSet::iterator BDDAndersPtsSet::begin() const {
  refreshCache();
  return cache->begin();
}

BDDAndersPtsSet::iterator BDDAndersPtsSet::end() const {
  refreshCache();
  return cache->end();
}
