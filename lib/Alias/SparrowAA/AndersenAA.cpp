#include "Alias/SparrowAA/AndersenAA.h"

#include <vector>

#include <llvm/IR/Module.h>

using namespace llvm;

static inline bool isSetContainingOnly(const AndersPtsSet &set, NodeIndex i) {
  return (set.getSize() == 1) && (*set.begin() == i);
}

AliasResult AndersenAAResult::andersenAlias(const Value *v1, const Value *v2) {
  std::vector<NodeIndex> n1List, n2List;
  (anders.nodeFactory).getValueNodesFor(v1, n1List);
  (anders.nodeFactory).getValueNodesFor(v2, n2List);

  for (auto n1 : n1List) {
    NodeIndex rep1 = (anders.nodeFactory).getMergeTarget(n1);
    for (auto n2 : n2List) {
      NodeIndex rep2 = (anders.nodeFactory).getMergeTarget(n2);
      if (rep1 == rep2)
        return AliasResult::MustAlias;
    }
  }

  AndersPtsSet s1, s2;
  if (!anders.getPointsToSet(v1, s1) || !anders.getPointsToSet(v2, s2))
    return AliasResult::MayAlias;

  NodeIndex nullObj = (anders.nodeFactory).getNullObjectNode();
  bool isNull1 = isSetContainingOnly(s1, nullObj);
  bool isNull2 = isSetContainingOnly(s2, nullObj);
  if (isNull1 || isNull2)
    return AliasResult::NoAlias;

  if (s1.getSize() == 1 && s2.getSize() == 1 && *s1.begin() == *s2.begin())
    return AliasResult::MustAlias;

  if (s1.intersectWith(s2))
    return AliasResult::MayAlias;

  return AliasResult::NoAlias;
}

AliasResult AndersenAAResult::alias(const MemoryLocation &l1,
                                    const MemoryLocation &l2) {
  if (l1.Size == 0 || l2.Size == 0)
    return AliasResult::NoAlias;

  const Value *v1 = (l1.Ptr)->stripPointerCasts();
  const Value *v2 = (l2.Ptr)->stripPointerCasts();

  if (!v1->getType()->isPointerTy() || !v2->getType()->isPointerTy())
    return AliasResult::NoAlias;

  if (v1 == v2)
    return AliasResult::MustAlias;

  return andersenAlias(v1, v2);
}

bool AndersenAAResult::pointsToConstantMemory(const MemoryLocation &loc,
                                              bool orLocal) {
  AndersPtsSet ptsSet;
  if (!anders.getPointsToSet(loc.Ptr, ptsSet))
    return false;

  for (auto const &idx : ptsSet) {
    if (const Value *val = (anders.nodeFactory).getValueForNode(idx)) {
      if (!isa<GlobalValue>(val) || (isa<GlobalVariable>(val) &&
                                     !cast<GlobalVariable>(val)->isConstant()))
        return false;
    } else {
      if (idx != (anders.nodeFactory).getNullObjectNode())
        return false;
    }
  }

  return true;
}

AndersenAAResult::AndersenAAResult(const Module &m)
    : anders(m, getSelectedAndersenContextPolicy()) {}

AndersenAAResult::AndersenAAResult(const Module &m, ContextPolicy policy)
    : anders(m, policy) {}

AndersenAAResult::AndersenAAResult(const Module &m, unsigned kCallSite)
    : anders(m, makeContextPolicy(kCallSite)) {}

// New Pass Manager implementation
AnalysisKey AndersenAA::Key;

AndersenAAResult AndersenAA::run(Module &M, ModuleAnalysisManager &) {
  return AndersenAAResult(M);
}