#include "Alias/TPA/Context/KLimitContext.h"
#include "Alias/TPA/PointerAnalysis/Engine/GlobalState.h"
#include "Alias/TPA/PointerAnalysis/Engine/StorePruner.h"
#include "Alias/TPA/PointerAnalysis/Engine/TransferFunction.h"
#include "Alias/TPA/PointerAnalysis/MemoryModel/MemoryManager.h"
#include "Alias/TPA/PointerAnalysis/MemoryModel/PointerManager.h"
#include "Alias/TPA/PointerAnalysis/Program/SemiSparseProgram.h"
#include "Alias/TPA/Util/IO/PointerAnalysis/Printer.h"

#include <llvm/Support/raw_ostream.h>

using namespace llvm;

namespace tpa {

static inline size_t countPointerArguments(const llvm::Function *f) {
  size_t ret = 0;
  for (auto &arg : f->args()) {
    if (arg.getType()->isPointerTy())
      ++ret;
  }
  return ret;
};

std::vector<const llvm::Function *>
TransferFunction::findFunctionInPtsSet(PtsSet pSet,
                                       const CallCFGNode &callNode) {
  auto callees = std::vector<const llvm::Function *>();

  // Two cases here
  if (pSet.has(MemoryManager::getUniversalObject())) {
    // If funSet contains unknown location, then we can't really derive callees
    // based on the points-to set Instead, guess callees based on the number of
    // arguments
    auto defaultTargets = globalState.getSemiSparseProgram().addr_taken_funcs();
    std::copy_if(defaultTargets.begin(), defaultTargets.end(),
                 std::back_inserter(callees), [&callNode](const Function *f) {
                   bool isArgMatch =
                       f->isVarArg() ||
                       countPointerArguments(f) == callNode.getNumArgument();
                   bool isRetMatch = (f->getReturnType()->isPointerTy()) !=
                                     (callNode.getDest() == nullptr);
                   return isArgMatch && isRetMatch;
                 });
  } else {
    for (const auto *obj : pSet) {
      if (obj->isFunctionObject())
        callees.emplace_back(obj->getAllocSite().getFunction());
    }
  }

  return callees;
}

std::vector<const llvm::Function *>
TransferFunction::resolveCallTarget(const context::Context *ctx,
                                    const CallCFGNode &callNode) {
  auto callees = std::vector<const llvm::Function *>();

  const auto *funPtr = globalState.getPointerManager().getPointer(
      ctx, callNode.getFunctionPointer());
  if (funPtr != nullptr) {
    auto const &funSet = globalState.getEnv().lookup(funPtr);
    if (!funSet.empty())
      callees = findFunctionInPtsSet(funSet, callNode);
  }

  return callees;
}

std::vector<PtsSet>
TransferFunction::collectArgumentPtsSets(const context::Context *ctx,
                                         const CallCFGNode &callNode,
                                         size_t numParams) {
  std::vector<PtsSet> result;
  result.reserve(numParams);

  auto &ptrManager = globalState.getPointerManager();
  auto &env = globalState.getEnv();
  auto argItr = callNode.begin();
  for (auto i = 0u; i < numParams; ++i) {
    const auto *argPtr = ptrManager.getPointer(ctx, *argItr);
    if (argPtr == nullptr)
      break;

    auto const &pSet = env.lookup(argPtr);
    if (pSet.empty())
      break;

    result.emplace_back(pSet);
    ++argItr;
  }

  return result;
}

bool TransferFunction::updateParameterPtsSets(
    const FunctionContext &fc, const std::vector<PtsSet> &argSets) {
  auto changed = false;

  auto &ptrManager = globalState.getPointerManager();
  auto &env = globalState.getEnv();
  const auto *newCtx = fc.getContext();
  const auto *paramItr = fc.getFunction()->arg_begin();
  for (auto const &pSet : argSets) {
    assert(paramItr != fc.getFunction()->arg_end());
    while (!paramItr->getType()->isPointerTy()) {
      ++paramItr;
      assert(paramItr != fc.getFunction()->arg_end());
    }
    const Argument *paramVal = paramItr;
    ++paramItr;

    const auto *paramPtr = ptrManager.getOrCreatePointer(newCtx, paramVal);
    changed |= env.weakUpdate(paramPtr, pSet);
  }

  return changed;
}

// Return true if eval succeeded
std::pair<bool, bool>
TransferFunction::evalCallArguments(const context::Context *ctx,
                                    const CallCFGNode &callNode,
                                    const FunctionContext &fc) {
  const auto numParams = countPointerArguments(fc.getFunction());
  assert(callNode.getNumArgument() >= numParams);
  ;
  const auto argSets = collectArgumentPtsSets(ctx, callNode, numParams);
  if (argSets.size() < numParams)
    return std::make_pair(false, false);

  auto envChanged = updateParameterPtsSets(fc, argSets);
  return std::make_pair(true, envChanged);
}

void TransferFunction::evalInternalCall(const context::Context *ctx,
                                        const CallCFGNode &callNode,
                                        const FunctionContext &fc,
                                        EvalResult &evalResult,
                                        bool callGraphUpdated) {
  const auto *tgtCFG =
      globalState.getSemiSparseProgram().getCFGForFunction(*fc.getFunction());
  assert(tgtCFG != nullptr);
  const auto *tgtEntryNode = tgtCFG->getEntryNode();

  bool isValid, envChanged;
  std::tie(isValid, envChanged) = evalCallArguments(ctx, callNode, fc);
  if (!isValid)
    return;
  if (envChanged || callGraphUpdated) {
    evalResult.addTopLevelProgramPoint(
        ProgramPoint(fc.getContext(), tgtEntryNode));
  }

  auto prunedStore =
      StorePruner(globalState.getEnv(), globalState.getPointerManager(),
                  globalState.getMemoryManager())
          .pruneStore(*localState, ProgramPoint(ctx, &callNode));
  auto &newStore = evalResult.getNewStore(std::move(prunedStore));
  evalResult.addMemLevelProgramPoint(
      ProgramPoint(fc.getContext(), tgtEntryNode), newStore);

  // Force enqueuing the direct successors of the call
  if (!tgtCFG->doesNotReturn())
    addMemLevelSuccessors(ProgramPoint(ctx, &callNode), *localState,
                          evalResult);
}

void TransferFunction::evalCallNode(const ProgramPoint &pp,
                                    EvalResult &evalResult) {
  const auto *ctx = pp.getContext();
  auto const &callNode = static_cast<const CallCFGNode &>(*pp.getCFGNode());

  const auto callees = resolveCallTarget(ctx, callNode);
  if (callees.empty())
    return;

  for (const auto *f : callees) {
    // Update call graph first
    const auto *callsite = callNode.getCallSite();
    const auto *newCtx = context::KLimitContext::pushContext(ctx, callsite);
    auto callTgt = FunctionContext(newCtx, f);
    bool callGraphUpdated = globalState.getCallGraph().insertEdge(
        ProgramPoint(ctx, &callNode), callTgt);

    // Check whether f is an external library call
    if (f->isDeclaration())
      evalExternalCall(ctx, callNode, callTgt, evalResult);
    else
      evalInternalCall(ctx, callNode, callTgt, evalResult, callGraphUpdated);
  }
}

} // namespace tpa
