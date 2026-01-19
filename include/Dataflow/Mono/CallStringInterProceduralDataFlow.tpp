/**
 * Template definitions for the call-string inter-procedural engine.
 *
 * Kept in a separate TPP file to avoid link-time template issues.
 */

#ifndef ANALYSIS_CALLSTRING_INTERPROCEDURAL_DATAFLOW_TPP_
#define ANALYSIS_CALLSTRING_INTERPROCEDURAL_DATAFLOW_TPP_

// Keep includes minimal here; templates are instantiated from the header.
#include "llvm/IR/Instructions.h"

namespace dataflow {

using namespace llvm;

template <unsigned K, typename ContainerT>
bool CallStringInterProceduralDataFlowEngine<K, ContainerT>::
    isCallToDefinedFunction(
    Instruction *Inst) {
  auto *Call = dyn_cast<CallBase>(Inst);
  if (Call == nullptr) {
    return false;
  }

  auto *Callee = Call->getCalledFunction();
  return Callee != nullptr && !Callee->isDeclaration() && !Callee->empty();
}

template <unsigned K, typename ContainerT>
Function *CallStringInterProceduralDataFlowEngine<K, ContainerT>::
    getDirectCallee(
    Instruction *Inst) {
  auto *Call = dyn_cast<CallBase>(Inst);
  if (Call == nullptr) {
    return nullptr;
  }
  return Call->getCalledFunction();
}

template <unsigned K, typename ContainerT>
std::vector<Instruction *>
CallStringInterProceduralDataFlowEngine<K, ContainerT>::getReturnInstructions(
    Function *F) {
  std::vector<Instruction *> Returns;
  if (F == nullptr) {
    return Returns;
  }

  for (auto &BB : *F) {
    if (auto *Ret = llvm::dyn_cast<llvm::ReturnInst>(BB.getTerminator())) {
      Returns.push_back(Ret);
    }
  }

  return Returns;
}

template <unsigned K, typename ContainerT>
std::vector<Instruction *>
CallStringInterProceduralDataFlowEngine<K, ContainerT>::normalSuccessors(
    Instruction *Inst) {
  std::vector<Instruction *> Succs;

  if (Inst->isTerminator()) {
    for (auto *SuccBB : llvm::successors(Inst->getParent())) {
      Succs.push_back(getFirstInstruction(SuccBB));
    }
    return Succs;
  }

  if (auto *Next = Inst->getNextNode()) {
    Succs.push_back(Next);
  }
  return Succs;
}

template <unsigned K, typename ContainerT>
std::vector<Instruction *>
CallStringInterProceduralDataFlowEngine<K, ContainerT>::normalPredecessors(
    Instruction *Inst) {
  std::vector<Instruction *> Preds;
  auto *BB = Inst->getParent();

  if (Inst != &*BB->begin()) {
    Preds.push_back(Inst->getPrevNode());
    return Preds;
  }

  for (auto *PredBB : llvm::predecessors(BB)) {
    Preds.push_back(PredBB->getTerminator());
  }
  return Preds;
}

template <unsigned K, typename ContainerT>
std::vector<Instruction *>
CallStringInterProceduralDataFlowEngine<K, ContainerT>::
    continuationInstructions(
    Instruction *CallInst) {
  std::vector<Instruction *> Continuations;

  if (auto *Invoke = llvm::dyn_cast<llvm::InvokeInst>(CallInst)) {
    auto *NormalDest = Invoke->getNormalDest();
    Continuations.push_back(getFirstInstruction(NormalDest));
    return Continuations;
  }

  if (auto *Next = CallInst->getNextNode()) {
    Continuations.push_back(Next);
  }
  return Continuations;
}

template <unsigned K, typename ContainerT>
void CallStringInterProceduralDataFlowEngine<K, ContainerT>::computeGenKill(
    Module *M, std::function<void(Instruction *, ResultTy *)> computeGEN,
    std::function<void(Instruction *, ResultTy *)> computeKILL, ResultTy *DF) {

  for (auto &F : *M) {
    if (F.isDeclaration()) {
      continue;
    }
    for (auto &BB : F) {
      for (auto &I : BB) {
        computeGEN(&I, DF);
        computeKILL(&I, DF);
      }
    }
  }
}

template <unsigned K, typename ContainerT>
void CallStringInterProceduralDataFlowEngine<K, ContainerT>::ensureInitialized(
    const ContextKey &Key,
    std::function<void(Instruction *, ContainerT &)> initializeIN,
    std::function<void(Instruction *, ContainerT &)> initializeOUT,
    ResultTy *DF) {

  if (DF->hasContext(Key)) {
    return;
  }
  auto &INSet = DF->IN(Key);
  auto &OUTSet = DF->OUT(Key);
  initializeIN(Key.Inst, INSet);
  initializeOUT(Key.Inst, OUTSet);
}

template <unsigned K, typename ContainerT>
std::vector<typename CallStringInterProceduralDataFlowEngine<
    K, ContainerT>::ContextKey>
CallStringInterProceduralDataFlowEngine<K, ContainerT>::successors(
    const ContextKey &Key) {
  std::vector<ContextKey> Result;
  auto *Inst = Key.Inst;
  auto Ctx = Key.Ctx;

  if (auto *Ret = llvm::dyn_cast<llvm::ReturnInst>(Inst)) {
    if (!Ctx.empty()) {
      auto CallerCtx = Ctx;
      auto *CallInst = CallerCtx.pop_back();
      for (auto *Cont : continuationInstructions(CallInst)) {
        Result.push_back({Cont, CallerCtx});
      }
    }
    return Result;
  }

  if (isCallToDefinedFunction(Inst)) {
    auto *Callee = getDirectCallee(Inst);
    if (Callee != nullptr && !Callee->isDeclaration()) {
      Context CalleeCtx = Ctx;
      CalleeCtx.push_back(Inst);
      Result.push_back({getFirstInstruction(&Callee->getEntryBlock()),
                        CalleeCtx});
      return Result;
    }
  }

  for (auto *SuccInst : normalSuccessors(Inst)) {
    Result.push_back({SuccInst, Ctx});
  }
  return Result;
}

template <unsigned K, typename ContainerT>
std::vector<typename CallStringInterProceduralDataFlowEngine<
    K, ContainerT>::ContextKey>
CallStringInterProceduralDataFlowEngine<K, ContainerT>::predecessors(
    const ContextKey &Key,
    const std::map<Instruction *, std::vector<Instruction *>> &CallToReturns,
    const std::map<Instruction *, std::vector<Instruction *>> &ContinuationToCalls) {
  std::vector<ContextKey> Result;
  auto *Inst = Key.Inst;
  auto Ctx = Key.Ctx;

  if (isFunctionEntry(Inst)) {
    if (!Ctx.empty()) {
      auto CallerCtx = Ctx;
      auto *CallInst = CallerCtx.pop_back();
      Result.push_back({CallInst, CallerCtx});
    }
    return Result;
  }

  // Continuation after a call: add predecessors from callee returns
  auto ContIt = ContinuationToCalls.find(Inst);
  if (ContIt != ContinuationToCalls.end()) {
    for (auto *CallInst : ContIt->second) {
      auto CallToRetIt = CallToReturns.find(CallInst);
      if (CallToRetIt == CallToReturns.end()) {
        continue;
      }
      Context RetCtx = Ctx;
      RetCtx.push_back(CallInst);
      for (auto *RetInst : CallToRetIt->second) {
        Result.push_back({RetInst, RetCtx});
      }
    }
  }

  for (auto *PredInst : normalPredecessors(Inst)) {
    Result.push_back({PredInst, Ctx});
  }
  return Result;
}

template <unsigned K, typename ContainerT>
typename CallStringInterProceduralDataFlowEngine<K, ContainerT>::ResultTy *
CallStringInterProceduralDataFlowEngine<K, ContainerT>::applyForward(
    Function *Entry,
    std::function<void(Instruction *, ResultTy *)> computeGEN,
    std::function<void(Instruction *, ResultTy *)> computeKILL,
    std::function<void(Instruction *Inst, ContainerT &IN)> initializeIN,
    std::function<void(Instruction *Inst, ContainerT &OUT)>
        initializeOUT,
    std::function<void(Instruction *Inst, Instruction *PredInst,
                       const Context &PredCtx, ContainerT &IN,
                       ResultTy *DF)>
        computeIN,
    std::function<void(Instruction *Inst, const Context &Ctx, ContainerT &OUT,
                       ResultTy *DF)>
        computeOUT) {

  if (Entry == nullptr || Entry->isDeclaration()) {
    return nullptr;
  }

  auto *Module = Entry->getParent();
  auto *DF = new ResultTy();

  computeGenKill(Module, computeGEN, computeKILL, DF);

  // Pre-compute return sites for direct calls
  std::map<Instruction *, std::vector<Instruction *>> CallToReturns;
  std::map<Instruction *, std::vector<Instruction *>> ContinuationToCalls;
  for (auto &F : *Module) {
    if (F.isDeclaration()) {
      continue;
    }
    for (auto &BB : F) {
      for (auto &I : BB) {
        if (isCallToDefinedFunction(&I)) {
          auto *Callee = getDirectCallee(&I);
          CallToReturns[&I] = getReturnInstructions(Callee);
          for (auto *Cont : continuationInstructions(&I)) {
            ContinuationToCalls[Cont].push_back(&I);
          }
        } else if (auto *Invoke = llvm::dyn_cast<llvm::InvokeInst>(&I)) {
          // Even for undefined callees, the invoke still continues.
          for (auto *Cont : continuationInstructions(&I)) {
            ContinuationToCalls[Cont].push_back(&I);
          }
        }
      }
    }
  }

  WorkQueue Queue;
  std::set<ContextKey> InQueue;

  auto Enqueue = [&](const ContextKey &Key) {
    if (InQueue.insert(Key).second) {
      Queue.push_back(Key);
    }
  };

  Context EmptyCtx;
  ContextKey EntryKey{getFirstInstruction(&Entry->getEntryBlock()), EmptyCtx};
  Enqueue(EntryKey);

  while (!Queue.empty()) {
    ContextKey Current = Queue.front();
    Queue.pop_front();
    InQueue.erase(Current);

    ensureInitialized(Current, initializeIN, initializeOUT, DF);

    auto &InSet = DF->IN(Current);
    auto OldIn = InSet;

    for (auto PredKey : predecessors(Current, CallToReturns, ContinuationToCalls)) {
      ensureInitialized(PredKey, initializeIN, initializeOUT, DF);
      computeIN(Current.Inst, PredKey.Inst, PredKey.Ctx, InSet, DF);
    }

    auto &OutSet = DF->OUT(Current);
    auto OldOut = OutSet;
    computeOUT(Current.Inst, Current.Ctx, OutSet, DF);

    if (!(OutSet == OldOut) || !(InSet == OldIn)) {
      for (auto SuccKey : successors(Current)) {
        Enqueue(SuccKey);
      }
    }
  }

  return DF;
}

} // namespace dataflow

#endif // ANALYSIS_CALLSTRING_INTERPROCEDURAL_DATAFLOW_TPP_
