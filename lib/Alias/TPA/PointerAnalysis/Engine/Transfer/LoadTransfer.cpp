#include "Alias/TPA/PointerAnalysis/Engine/GlobalState.h"
#include "Alias/TPA/PointerAnalysis/Engine/TransferFunction.h"
#include "Alias/TPA/PointerAnalysis/MemoryModel/MemoryManager.h"
#include "Alias/TPA/PointerAnalysis/MemoryModel/PointerManager.h"

namespace tpa {

PtsSet TransferFunction::loadFromPointer(const Pointer *ptr,
                                         const Store &store) {
  assert(ptr != nullptr);

  const auto *uObj = MemoryManager::getUniversalObject();
  auto const &srcSet = globalState.getEnv().lookup(ptr);
  if (!srcSet.empty()) {
    std::vector<PtsSet> srcSets;
    srcSets.reserve(srcSet.size());

    for (const auto *obj : srcSet) {
      auto const &objSet = store.lookup(obj);
      if (!objSet.empty()) {
        srcSets.emplace_back(objSet);
        if (objSet.has(uObj))
          break;
      }
    }

    return PtsSet::mergeAll(srcSets);
  }

  return PtsSet::getSingletonSet(uObj);
}

void TransferFunction::evalLoadNode(const ProgramPoint &pp,
                                    EvalResult &evalResult) {
  const auto *ctx = pp.getContext();
  auto const &loadNode = static_cast<const LoadCFGNode &>(*pp.getCFGNode());

  auto &ptrManager = globalState.getPointerManager();
  const auto *srcPtr = ptrManager.getPointer(ctx, loadNode.getSrc());
  if (srcPtr == nullptr)
    return;

  // assert(srcPtr != nullptr && "LoadNode is evaluated before its src operand
  // becomes available");
  const auto *dstPtr = ptrManager.getOrCreatePointer(ctx, loadNode.getDest());

  const auto resSet = loadFromPointer(srcPtr, *localState);
  auto envChanged = globalState.getEnv().strongUpdate(dstPtr, resSet);

  if (envChanged)
    addTopLevelSuccessors(pp, evalResult);
  addMemLevelSuccessors(pp, *localState, evalResult);
}

} // namespace tpa
