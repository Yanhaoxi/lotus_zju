/*
FIXME: this is a pretty much unfinished pass.
*/
#include "IR/SSI/SSI.h"

using namespace llvm;

namespace ssi
{
  char SSIPass::ID = 0;
  bool DEBUG = false;

  static RegisterPass<SSIPass>
      SSI("ssi", "Static Single Information Construction", false, true);

  void SSIPass::getAnalysisUsage(AnalysisUsage &AU) const
  {
    AU.addRequired<PostDominatorTreeWrapperPass>();
    AU.setPreservesAll();
  }

  bool SSIPass::runOnFunction(Function &F)
  {
    Info.clear();
    auto &PDT = getAnalysis<PostDominatorTreeWrapperPass>().getPostDomTree();
    findSigmaSites(F, PDT);
    if (DEBUG)
      Info.dump(F);
    return false;
  }

  bool SSIPass::needsSigmaAt(ArrayRef<BasicBlock *> Succs, PostDominatorTree &PDT)
  {
    if (Succs.size() <= 1)
      return false;
    BasicBlock *First = Succs.front();
    for (BasicBlock *Succ : Succs)
    {
      if (!PDT.dominates(PDT.getRootNode(), PDT.getNode(Succ)))
        return true;
      // Require that all successors belong to the same post-dominance region;
      // if any successor is not post-dominated by the same immediate post-dominator
      // as the others, we conservatively place a sigma.
      if (Succ != First)
      {
        auto *N1 = PDT.getNode(First);
        auto *N2 = PDT.getNode(Succ);
        if (!N1 || !N2)
          return true;
        auto *IDom1 = N1->getIDom();
        auto *IDom2 = N2->getIDom();
        if (IDom1 != IDom2)
          return true;
      }
    }
    return false;
  }

  void SSIPass::findSigmaSites(Function &F, PostDominatorTree &PDT)
  {
    for (auto &BB : F)
    {
      Instruction *TI = BB.getTerminator();
      if (!TI)
        continue;
      SmallVector<BasicBlock *, 4> Succs;
      for (auto SI = succ_begin(&BB), SE = succ_end(&BB); SI != SE; ++SI)
        Succs.push_back(*SI);
      if (needsSigmaAt(Succs, PDT))
        Info.addPlacement(TI, Succs);
    }
  }
}


