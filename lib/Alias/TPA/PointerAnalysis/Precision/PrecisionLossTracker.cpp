#include "Alias/TPA/PointerAnalysis/Precision/PrecisionLossTracker.h"

#include "Alias/TPA/PointerAnalysis/Engine/GlobalState.h"
#include "Alias/TPA/PointerAnalysis/Engine/WorkList.h"
#include "Alias/TPA/PointerAnalysis/MemoryModel/MemoryManager.h"
#include "Alias/TPA/PointerAnalysis/MemoryModel/Pointer.h"
#include "Alias/TPA/PointerAnalysis/MemoryModel/PointerManager.h"
#include "Alias/TPA/PointerAnalysis/Precision/TrackerGlobalState.h"
#include "Alias/TPA/PointerAnalysis/Precision/ValueDependenceTracker.h"
#include "Alias/TPA/PointerAnalysis/Program/SemiSparseProgram.h"

#include <llvm/IR/Argument.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instruction.h>

using namespace context;
using namespace llvm;

namespace tpa {

static const Function *getFunction(const Value *val) {
  if (const auto *arg = dyn_cast<Argument>(val))
    return arg->getParent();
  else if (const auto *inst = dyn_cast<Instruction>(val))
    return inst->getParent()->getParent();
  else
    return nullptr;
}

PrecisionLossTracker::ProgramPointList
PrecisionLossTracker::getProgramPointsFromPointers(const PointerList &ptrs) {
  ProgramPointList list;
  list.reserve(ptrs.size());

  for (const auto *ptr : ptrs) {
    const auto *value = ptr->getValue();

    if (const auto *func = getFunction(value)) {
      const auto *cfg =
          globalState.getSemiSparseProgram().getCFGForFunction(*func);
      assert(cfg != nullptr);

      const auto *node = cfg->getCFGNodeForValue(value);
      assert(node != nullptr);
      list.push_back(ProgramPoint(ptr->getContext(), node));
    }
  }

  return list;
}

namespace {

class ImprecisionTracker {
private:
  TrackerGlobalState &globalState;

  PtsSet getPtsSet(const Context *, const Value *);
  bool morePrecise(const PtsSet &, const PtsSet &);

  void checkCalleeDependencies(const ProgramPoint &, ProgramPointSet &);
  void checkCallerDependencies(const ProgramPoint &, ProgramPointSet &);

public:
  ImprecisionTracker(TrackerGlobalState &s) : globalState(s) {}

  void runOnWorkList(BackwardWorkList &workList);
};

PtsSet ImprecisionTracker::getPtsSet(const Context *ctx, const Value *val) {
  const auto *ptr = globalState.getPointerManager().getPointer(ctx, val);
  assert(ptr != nullptr);
  return globalState.getEnv().lookup(ptr);
}

void ImprecisionTracker::runOnWorkList(BackwardWorkList &workList) {
  while (!workList.empty()) {
    const auto pp = workList.dequeue();
    if (!globalState.insertVisitedLocation(pp))
      continue;

    auto deps = ValueDependenceTracker(globalState.getCallGraph(),
                                       globalState.getSemiSparseProgram())
                    .getValueDependencies(pp);

    const auto *node = pp.getCFGNode();
    if (node->isCallNode())
      checkCalleeDependencies(pp, deps);
    else if (node->isEntryNode())
      checkCallerDependencies(pp, deps);

    for (auto const &succ : deps)
      workList.enqueue(succ);
  }
}

bool ImprecisionTracker::morePrecise(const PtsSet &lhs, const PtsSet &rhs) {
  const auto *uObj = MemoryManager::getUniversalObject();
  if (rhs.has(uObj))
    return !lhs.has(uObj);

  return lhs.size() < rhs.size();
}

void ImprecisionTracker::checkCalleeDependencies(const ProgramPoint &pp,
                                                 ProgramPointSet &deps) {
  assert(pp.getCFGNode()->isCallNode());
  const auto *callNode = static_cast<const CallCFGNode *>(pp.getCFGNode());
  const auto *dstVal = callNode->getDest();
  if (dstVal == nullptr)
    return;

  const auto dstSet = getPtsSet(pp.getContext(), dstVal);
  assert(!dstSet.empty());

  ProgramPointSet newSet;
  bool needPrecision = false;
  for (auto const &retPoint : deps) {
    assert(retPoint.getCFGNode()->isReturnNode());
    const auto *retNode =
        static_cast<const ReturnCFGNode *>(retPoint.getCFGNode());
    const auto *retVal = retNode->getReturnValue();
    assert(retVal != nullptr);

    const auto retSet = getPtsSet(retPoint.getContext(), retVal);
    assert(!retSet.empty());
    if (morePrecise(retSet, dstSet))
      needPrecision = true;
    else
      newSet.insert(retPoint);
  }

  if (needPrecision) {
    globalState.addImprecisionSource(pp);
    deps.swap(newSet);
  }
}

void ImprecisionTracker::checkCallerDependencies(const ProgramPoint &pp,
                                                 ProgramPointSet &deps) {
  (void)pp;
  (void)deps;
  // TODO: Finish this
}

} // namespace

ProgramPointSet
PrecisionLossTracker::trackImprecision(const PointerList &ptrs) {
  ProgramPointSet ppSet;

  const auto ppList = getProgramPointsFromPointers(ptrs);
  auto workList = BackwardWorkList();
  for (auto const &pp : ppList)
    workList.enqueue(pp);

  TrackerGlobalState trackerState(
      globalState.getPointerManager(), globalState.getMemoryManager(),
      globalState.getSemiSparseProgram(), globalState.getEnv(),
      globalState.getCallGraph(), globalState.getExternalPointerTable(), ppSet);
  ImprecisionTracker(trackerState).runOnWorkList(workList);

  return ppSet;
}

} // namespace tpa
