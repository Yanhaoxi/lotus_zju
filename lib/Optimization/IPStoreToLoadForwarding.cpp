// Interprocedural store-to-load forwarding using MemorySSA instrumentation.
// Walks MemorySSA def-use chains to find a unique reaching store value for the
// same pointer. If exactly one value is found and no conflicts are seen, the
// load is replaced with that value.
//
// Pseudocode:
//   for each shadow.mem.load + following Load L:
//     targetPtr = stripCasts(L.ptr)
//     BFS over MemorySSA value starting at TLVar of load:
//       - on shadow.mem.store -> capture Store value if pointer matches target
//       - on shadow.mem.arg.mod/ref_mod/new -> follow non-primed
//       - on shadow.mem.in -> jump to callers via shadow.mem.arg.primed(idx)
//       - on PHI -> visit operands
//       - on arg.init/global.init/ref/out -> stop (base/unsupported)
//     if exactly one reaching value, rewrite L to that value and drop load call
#include "IR/MemorySSA/MemorySSA.h"

#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

#include <queue>
#include <set>

namespace previrt {
namespace transforms {

using namespace llvm;
using namespace analysis;

static cl::opt<bool> OnlySingletonForward(
    "ip-forward-only-singleton",
    cl::desc("IP Store-to-Load Forwarding: only singleton regions"),
    cl::Hidden, cl::init(true));

namespace {
struct ForwardSearchState {
  const Value *TargetPtr;
  Type *TargetTy;
  const MemorySSACallsManager &MMan;
  bool Conflict = false;
  const Value *ReachingStoreVal = nullptr;

  ForwardSearchState(const Value *Ptr, Type *Ty,
                     const MemorySSACallsManager &M)
      : TargetPtr(Ptr), TargetTy(Ty), MMan(M) {}

  bool merge(const Value *Candidate, const Value *StorePtr) {
    if (Conflict)
      return false;
    if (StorePtr != TargetPtr)
      return false;
    if (!Candidate || Candidate->getType() != TargetTy) {
      Conflict = true;
      return false;
    }
    if (!ReachingStoreVal) {
      ReachingStoreVal = Candidate;
      return true;
    }
    if (ReachingStoreVal != Candidate) {
      Conflict = true;
      return false;
    }
    return true;
  }
};

static void enqueueIfInstruction(std::queue<const Value *> &Q,
                                 const Value *V) {
  if (const Instruction *I = dyn_cast_or_null<const Instruction>(V)) {
    Q.push(I);
  }
}

static const Instruction *nextNonDebugInst(const Instruction *I) {
  if (!I)
    return nullptr;
  auto It = std::next(I->getIterator());
  auto End = I->getParent()->end();
  while (It != End && isa<DbgInfoIntrinsic>(&*It)) {
    ++It;
  }
  return (It == End) ? nullptr : &*It;
}

static void exploreFunIn(const CallBase *CB, const Function *F,
                         unsigned Idx, ForwardSearchState &State,
                         std::queue<const Value *> &Q) {
  for (auto &U : F->uses()) {
    if (const CallInst *CI = dyn_cast<CallInst>(U.getUser())) {
      const analysis::MemorySSACallSite *CS = State.MMan.getCallSite(CI);
      if (!CS)
        continue;
      if (Idx >= CS->numParams())
        continue;
      enqueueIfInstruction(Q, CS->getPrimed(Idx));
    }
  }
}

static bool findReachingStore(const Value *StartVal, const Function *CurF,
                              ForwardSearchState &State) {
  std::queue<const Value *> Q;
  std::set<const Value *> Visited;
  enqueueIfInstruction(Q, StartVal);

  while (!Q.empty() && !State.Conflict) {
    const Value *V = Q.front();
    Q.pop();
    if (!Visited.insert(V).second)
      continue;

    if (const CallBase *CB = dyn_cast<CallBase>(V)) {
      if (isMemSSAStore(CB, OnlySingletonForward)) {
        if (const Instruction *Next = nextNonDebugInst(CB)) {
          if (const auto *SI = dyn_cast<StoreInst>(Next)) {
            const Value *StorePtr = SI->getPointerOperand()->stripPointerCasts();
            State.merge(SI->getValueOperand(), StorePtr);
          }
        }
        continue;
      }

      if (isMemSSAArgMod(CB, OnlySingletonForward) ||
          isMemSSAArgRefMod(CB, OnlySingletonForward) ||
          isMemSSAArgNew(CB, OnlySingletonForward)) {
        enqueueIfInstruction(Q, CB->getArgOperand(1));
        continue;
      }

      if (isMemSSAFunIn(CB, OnlySingletonForward)) {
        int64_t Idx = getMemSSAParamIdx(CB);
        if (Idx >= 0) {
          exploreFunIn(CB, CurF, static_cast<unsigned>(Idx), State, Q);
        }
        continue;
      }

      if (isMemSSAArgInit(CB, OnlySingletonForward) ||
          isMemSSAGlobalInit(CB, OnlySingletonForward) ||
          isMemSSAArgRef(CB, OnlySingletonForward) ||
          isMemSSAFunOut(CB, OnlySingletonForward)) {
        // Base cases or unsupported edges for now.
        continue;
      }
    }

    if (const PHINode *PN = dyn_cast<PHINode>(V)) {
      for (const Value *Op : PN->incoming_values())
        enqueueIfInstruction(Q, Op);
    }
  }

  return State.ReachingStoreVal && !State.Conflict;
}
} // namespace

class IPStoreToLoadForwarding : public ModulePass {
public:
  static char ID;
  IPStoreToLoadForwarding() : ModulePass(ID) {}

  bool runOnModule(Module &M) override {
    if (M.begin() == M.end())
      return false;

    unsigned NumForwarded = 0;
    MemorySSACallsManager MMan(M, *this, OnlySingletonForward);

    for (Function &F : M) {
      if (F.isDeclaration())
        continue;
      for (BasicBlock &BB : F) {
        for (auto It = BB.begin(); It != BB.end();) {
          Instruction *Inst = &*It++;
          CallBase *CB = dyn_cast<CallBase>(Inst);
          if (!CB || !isMemSSALoad(CB, OnlySingletonForward))
            continue;
          if (It == BB.end())
            continue;
          LoadInst *LI = dyn_cast<LoadInst>(&*It);
          if (!LI)
            continue;

          const Value *Ptr = LI->getPointerOperand()->stripPointerCasts();
          ForwardSearchState State(Ptr, LI->getType(), MMan);
          if (!findReachingStore(CB->getArgOperand(1), LI->getFunction(), State))
            continue;

          LI->replaceAllUsesWith(const_cast<Value *>(State.ReachingStoreVal));
          ++It; // Advance past the load before erasing it.
          LI->eraseFromParent();
          if (CB->use_empty()) {
            CB->eraseFromParent();
          }
          NumForwarded++;
        }
      }
    }

    if (NumForwarded > 0) {
      errs() << "IP-Forward: forwarded " << NumForwarded << " loads\n";
    }
    return NumForwarded > 0;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
  }

  StringRef getPassName() const override {
    return "Interprocedural Store-to-Load Forwarding";
  }
};

char IPStoreToLoadForwarding::ID = 0;

static RegisterPass<IPStoreToLoadForwarding>
    X("ip-forward", "Interprocedural Store-to-Load Forwarding");

} // namespace transforms
} // namespace previrt
