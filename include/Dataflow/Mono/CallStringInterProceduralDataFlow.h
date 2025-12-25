/**
 * A lightweight call-string based inter-procedural monotone data-flow engine.
 *
 * The implementation keeps a separate IN/OUT lattice per (Instruction, CallString)
 * pair where the call string is bounded to length K. Call strings are represented
 * by the existing `CallStringCTX` helper which truncates on overflow.
 *
 * The API mirrors the intraprocedural `DataFlowEngine` but extends the transfer
 * functions to receive the predecessor context when computing IN. GEN/KILL are
 * still computed per-instruction (context-insensitive) which matches the
 * standard call-string formulation for monotone frameworks.
 *
 * Currently only forward analyses are provided; backward support can be plugged
 * in using the same building blocks if needed.
 */

#ifndef ANALYSIS_CALLSTRING_INTERPROCEDURAL_DATAFLOW_H_
#define ANALYSIS_CALLSTRING_INTERPROCEDURAL_DATAFLOW_H_

//#include "Utils/LLVM/SystemHeaders.h"
#include "Dataflow/Mono/CallStringCTX.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Instructions.h"

#include <deque>
#include <functional>
#include <map>
//#include <optional>
#include <queue>
#include <set>
#include <unordered_set>
#include <vector>

namespace dataflow {

/**
 * Context sensitive data-flow result.
 *
 * GEN/KILL are keyed only by Instruction* (shared across contexts).
 * IN/OUT are keyed by the pair (Instruction*, CallString).
 */
template <unsigned K> class ContextSensitiveDataFlowResult {
public:
  using Context = CallStringCTX<Instruction *, K>;

  struct ContextKey {
    Instruction *Inst;
    Context Ctx;

    bool operator<(const ContextKey &Rhs) const {
      if (Inst != Rhs.Inst) {
        return Inst < Rhs.Inst;
      }
      return Ctx < Rhs.Ctx;
    }
  };

  ContextSensitiveDataFlowResult() = default;

  std::set<Value *> &GEN(Instruction *Inst) { return Gens[Inst]; }
  std::set<Value *> &KILL(Instruction *Inst) { return Kills[Inst]; }

  std::set<Value *> &IN(const ContextKey &Key) { return Ins[Key]; }
  std::set<Value *> &OUT(const ContextKey &Key) { return Outs[Key]; }
  std::set<Value *> &IN(Instruction *Inst, const Context &Ctx) {
    return IN(ContextKey{Inst, Ctx});
  }
  std::set<Value *> &OUT(Instruction *Inst, const Context &Ctx) {
    return OUT(ContextKey{Inst, Ctx});
  }

  const std::set<Value *> &IN(const ContextKey &Key) const {
    auto It = Ins.find(Key);
    if (It == Ins.end()) {
      static std::set<Value *> EmptySet;
      return EmptySet;
    }
    return It->second;
  }

  const std::set<Value *> &OUT(const ContextKey &Key) const {
    auto It = Outs.find(Key);
    if (It == Outs.end()) {
      static std::set<Value *> EmptySet;
      return EmptySet;
    }
    return It->second;
  }
  const std::set<Value *> &IN(Instruction *Inst, const Context &Ctx) const {
    return IN(ContextKey{Inst, Ctx});
  }
  const std::set<Value *> &OUT(Instruction *Inst, const Context &Ctx) const {
    return OUT(ContextKey{Inst, Ctx});
  }

  bool hasContext(const ContextKey &Key) const {
    return Ins.find(Key) != Ins.end() || Outs.find(Key) != Outs.end();
  }

private:
  std::map<Instruction *, std::set<Value *>> Gens;
  std::map<Instruction *, std::set<Value *>> Kills;
  std::map<ContextKey, std::set<Value *>> Ins;
  std::map<ContextKey, std::set<Value *>> Outs;
};

/**
 * Call-string inter-procedural forward engine.
 *
 * K bounds the call-string length.
 */
template <unsigned K> class CallStringInterProceduralDataFlowEngine {
public:
  using ResultTy = ContextSensitiveDataFlowResult<K>;
  using Context = typename ResultTy::Context;
  using ContextKey = typename ResultTy::ContextKey;

  CallStringInterProceduralDataFlowEngine() = default;

  /**
   * Forward call-string analysis rooted at `Entry`.
   *
   * - computeGEN/KILL: per-instruction (context-insensitive) transformers.
   * - initializeIN/initializeOUT: called when a (Inst, Ctx) pair is first seen.
   * - computeIN: merges predecessor OUT into IN. Receives predecessor context.
   * - computeOUT: updates OUT for the current node using its IN/GEN/KILL/etc.
   */
  ResultTy *applyForward(
      Function *Entry,
      std::function<void(Instruction *, ResultTy *)> computeGEN,
      std::function<void(Instruction *, ResultTy *)> computeKILL,
      std::function<void(Instruction *Inst, std::set<Value *> &IN)>
          initializeIN,
      std::function<void(Instruction *Inst, std::set<Value *> &OUT)>
          initializeOUT,
      std::function<void(Instruction *Inst, Instruction *PredInst,
                         const Context &PredCtx, std::set<Value *> &IN,
                         ResultTy *DF)>
          computeIN,
      std::function<void(Instruction *Inst, const Context &Ctx,
                         std::set<Value *> &OUT, ResultTy *DF)>
          computeOUT);

  /**
   * Convenience overload with empty KILL sets.
   */
  ResultTy *applyForward(
      Function *Entry,
      std::function<void(Instruction *, ResultTy *)> computeGEN,
      std::function<void(Instruction *Inst, std::set<Value *> &IN)>
          initializeIN,
      std::function<void(Instruction *Inst, std::set<Value *> &OUT)>
          initializeOUT,
      std::function<void(Instruction *Inst, Instruction *PredInst,
                         const Context &PredCtx, std::set<Value *> &IN,
                         ResultTy *DF)>
          computeIN,
      std::function<void(Instruction *Inst, const Context &Ctx,
                         std::set<Value *> &OUT, ResultTy *DF)>
          computeOUT) {
    auto EmptyKill = [](Instruction *, ResultTy *) {};
    return applyForward(Entry, computeGEN, EmptyKill, initializeIN,
                        initializeOUT, computeIN, computeOUT);
  }

private:
  using WorkQueue = std::deque<ContextKey>;

  static bool isCallToDefinedFunction(Instruction *Inst);
  static Function *getDirectCallee(Instruction *Inst);

  static std::vector<Instruction *>
  getReturnInstructions(Function *F);

  static Instruction *
  getFirstInstruction(BasicBlock *BB) {
    return &*BB->begin();
  }

  static std::vector<Instruction *>
  normalSuccessors(Instruction *Inst);

  static std::vector<Instruction *>
  normalPredecessors(Instruction *Inst);

  static std::vector<Instruction *>
  continuationInstructions(Instruction *CallInst);

  static bool isFunctionEntry(Instruction *Inst) {
    auto *BB = Inst->getParent();
    return &BB->getParent()->getEntryBlock() == BB &&
           Inst == &*BB->begin();
  }

  void computeGenKill(Module *M,
                      std::function<void(Instruction *, ResultTy *)> computeGEN,
                      std::function<void(Instruction *, ResultTy *)> computeKILL,
                      ResultTy *DF);

  void ensureInitialized(const ContextKey &Key,
                         std::function<void(Instruction *, std::set<Value *> &)>
                             initializeIN,
                         std::function<void(Instruction *, std::set<Value *> &)>
                             initializeOUT,
                         ResultTy *DF);

  std::vector<ContextKey>
  predecessors(
      const ContextKey &Key,
      const std::map<Instruction *, std::vector<Instruction *>> &CallToReturns,
      const std::map<Instruction *, std::vector<Instruction *>> &ContinuationToCalls);

  std::vector<ContextKey>
  successors(const ContextKey &Key);
};

} // namespace dataflow

#include "Dataflow/Mono/CallStringInterProceduralDataFlow.tpp"

#endif // ANALYSIS_CALLSTRING_INTERPROCEDURAL_DATAFLOW_H_
