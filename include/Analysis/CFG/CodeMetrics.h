/*======================================================================*\
|  “ComplexityMetrics” – one-stop shop for quick-and-dirty       |

- Cyclomatic cmplexity: Measures the number of independent paths through code by counting decision points (if, while, for, case statements). Higher values mean more complex code that's harder to test
- Loop count / max nesting depth: Loop count tracks how many loops exist in code. Max nesting depth measures how deeply nested your control structures are (loops inside loops, ifs inside ifs). Deep nesting makes code hard to read and maintain.
- NPath complexity: Counts the total number of unique execution paths through a function, considering all possible combinations of branches and loops. It grows exponentially with nested conditions. Much larger than cyclomatic complexity
*/

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/CFG.h"
//#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/Function.h"
//#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/LegacyPassManager.h"
//#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
//#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"

#include <cmath>

using namespace llvm;

/*-------------------------------------------------------------*
 * 1. Cyclomatic Complexity                                    *
 *-------------------------------------------------------------*/
static unsigned calcCyclomaticComplexity(Function &F) {
  unsigned Blocks = 0, Edges = 0, Calls = 0;

  for (auto &BB : F) {
    ++Blocks;
    for (auto *Succ : successors(&BB)) {
      (void)Succ;
      ++Edges;
    }
    for (auto &I : BB)
      if (isa<CallInst>(I) || isa<InvokeInst>(I))
        ++Calls;
  }

  /*  V(G) = E – N + 2P,  P==1 for a single function                */
  return 2 + Calls + Edges - Blocks;
}

/*-------------------------------------------------------------*
 * 2. Loop count / max nesting depth                           *
 *-------------------------------------------------------------*/
struct LoopMetrics { unsigned NumLoops = 0, MaxDepth = 0; };

static void scanLoop(const Loop *L, unsigned Depth, LoopMetrics &M) {
  ++M.NumLoops;
  M.MaxDepth = std::max(M.MaxDepth, Depth);
  for (auto *Child : L->getSubLoops())
    scanLoop(Child, Depth + 1, M);
}

static LoopMetrics collectLoopMetrics(Function &F, LoopInfo &LI) {
  LoopMetrics M;
  for (auto *Top : LI)
    scanLoop(Top, 1, M);
  return M;
}

/*-------------------------------------------------------------*
 * 3. NPath complexity                                         *
 *-------------------------------------------------------------*/
static uint64_t paths(BasicBlock *BB, DenseMap<BasicBlock *, uint64_t> &Memo) {
  auto It = Memo.find(BB);
  if (It != Memo.end())
    return It->second;

  uint64_t Sum = 0;
  if (succ_empty(BB))
    Sum = 1;
  else
    for (auto *Succ : successors(BB))
      Sum += paths(Succ, Memo);

  return Memo[BB] = Sum;
}

static uint64_t nPath(Function &F) {
  DenseMap<BasicBlock *, uint64_t> Memo;
  return paths(&F.getEntryBlock(), Memo);
}



/*-------------------------------------------------------------*
 * Legacy PM glue                                           *
 *-------------------------------------------------------------*/
namespace {
struct ComplexityLegacy : public FunctionPass {
  static char ID;
  ComplexityLegacy() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override {
    auto &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
    auto CC = calcCyclomaticComplexity(F);
    //auto IS = collectInstStats(F);
    auto LM = collectLoopMetrics(F, LI);
    auto NP = nPath(F);

    errs() << "== " << F.getName() << " ==\n"
           << "  Cyclomatic    : " << CC << '\n'
           << "  NPath         : " << NP << '\n'
           << "  Loops         : " << LM.NumLoops
           << "  (max depth " << LM.MaxDepth << ")\n";

    return false; /* analysis pass */
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
    AU.addRequired<LoopInfoWrapperPass>();
    AU.addPreserved<LoopInfoWrapperPass>();
  }
};
} // end anonymous namespace

char ComplexityLegacy::ID = 0;
static RegisterPass<ComplexityLegacy>
    X("complexity-legacy", "Complexity metrics (legacy PM)",
      /*cfgOnly=*/false, /*is_analysis=*/true);