#include "IR/MemorySSA/MemorySSA.h"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

namespace previrt {
namespace transforms {

using namespace llvm;
using namespace analysis;

static cl::opt<bool> OnlySingletonRLE(
    "ip-rle-only-singleton",
    cl::desc("IP RLE: consider only singleton memory regions"), cl::Hidden,
    cl::init(true));

// Interprocedural redundant load elimination using MemorySSA instrumentation.
// Conservative: only removes repeated loads within a basic block when the
// MemorySSA version (TLVar) and pointer operand are identical and there are no
// intervening memory writes. This benefits interprocedural code because TLVars
// already encode effects across calls.
//
// Pseudocode:
//   for each basic block BB:
//     seen = {} // (TLVar, Ptr) -> dominating load
//     for inst I in BB:
//       if I mayReadOrWriteMemory: seen.clear()
//       if I is shadow.mem.load and next inst is Load L:
//         key = (TLVar, stripCasts(L.ptr))
//         if key in seen: replace L with seen[key], drop L and maybe load call
//         else: seen[key] = L
class IPRedundantLoadElimination : public ModulePass {
public:
  static char ID;
  IPRedundantLoadElimination() : ModulePass(ID) {}

  bool runOnModule(Module &M) override {
    if (M.begin() == M.end())
      return false;

    unsigned NumRemoved = 0;
    for (Function &F : M) {
      if (F.isDeclaration())
        continue;
      for (BasicBlock &BB : F) {
        // Map from (TLVar, Ptr) to the dominating load instruction.
        DenseMap<std::pair<const Value *, const Value *>, LoadInst *>
            SeenLoads;
        auto resetSeen = [&]() { SeenLoads.clear(); };

        for (auto It = BB.begin(), Et = BB.end(); It != Et; ++It) {
          Instruction *I = &*It;

          // Any instruction that may read/write memory can invalidate cached
          // loads (to stay conservative).
          if (I->mayReadOrWriteMemory()) {
            resetSeen();
          }

          CallBase *CB = dyn_cast<CallBase>(I);
          if (!CB || !isMemSSALoad(CB, OnlySingletonRLE))
            continue;

          auto NextIt = std::next(It);
          if (NextIt == BB.end())
            continue;
          LoadInst *LI = dyn_cast<LoadInst>(&*NextIt);
          if (!LI)
            continue;

          const Value *TLVar = CB->getArgOperand(1);
          const Value *Ptr = LI->getPointerOperand()->stripPointerCasts();
          auto Key = std::make_pair(TLVar, Ptr);

          auto Found = SeenLoads.find(Key);
          if (Found == SeenLoads.end()) {
            SeenLoads.insert({Key, LI});
            continue;
          }

          // We have an earlier dominating load with the same TLVar and pointer.
          LoadInst *DomLoad = Found->second;
          LI->replaceAllUsesWith(DomLoad);
          LI->eraseFromParent();
          if (CB->use_empty()) {
            CB->eraseFromParent();
            // Adjust iterator because we removed the current load's
            // shadow.mem.load.
            It = std::prev(NextIt);
          }
          NumRemoved++;
        }
      }
    }

    if (NumRemoved > 0) {
      errs() << "IP-RLE: removed " << NumRemoved << " redundant loads\n";
    }
    return NumRemoved > 0;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
  }

  StringRef getPassName() const override {
    return "Interprocedural Redundant Load Elimination";
  }
};

char IPRedundantLoadElimination::ID = 0;

static RegisterPass<IPRedundantLoadElimination>
    X("ip-rle", "Interprocedural Redundant Load Elimination");

} // namespace transforms
} // namespace previrt
