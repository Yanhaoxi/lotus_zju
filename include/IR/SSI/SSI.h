/*
Static Single Information (SSI) IR

SSI additionally guarantees that
- Every definition of a variable dominates all its uses (SSA property)
- Every use of a variable post-dominates all its reaching definitions

Static Single Information (SSI) form = SSA + σ-functions
1. Start from SSA form.
2. Compute the iterated post-dominance frontier (analogous to dominance frontier) to decide where σ’s are needed.
3. Insert σ-functions at each control-flow split whose successors are not in the same post-dom tree region.
4. Rename variables again to give unique names to σ results (mirrors SSA renaming).

*/

#pragma once

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/raw_ostream.h"

namespace ssi
{
  struct SigmaPlacement
  {
    llvm::Instruction *Term = nullptr; // terminator instruction location
    llvm::SmallVector<llvm::BasicBlock *, 4> Successors;
  };

  class SSIFunctionInfo
  {
    public:
      void clear()
      {
        Placements.clear();
      }

      void addPlacement(llvm::Instruction *Term, llvm::ArrayRef<llvm::BasicBlock *> Succs)
      {
        SigmaPlacement P;
        P.Term = Term;
        P.Successors.assign(Succs.begin(), Succs.end());
        Placements.push_back(std::move(P));
      }

      const llvm::SmallVector<SigmaPlacement, 8> &getPlacements() const { return Placements; }

      void dump(const llvm::Function &F) const
      {
        llvm::errs() << "[SSI] Function '" << F.getName() << "' has " << Placements.size() << " sigma site(s)\n";
        for (const auto &P : Placements)
        {
          llvm::errs() << "  - at terminator: ";
          if (P.Term)
            llvm::errs() << *P.Term << "\n";
          else
            llvm::errs() << "<null>\n";
          for (auto *Succ : P.Successors)
          {
            llvm::errs() << "      succ: ";
            if (Succ && Succ->getName().size())
              llvm::errs() << Succ->getName();
            else
              llvm::errs() << "<unnamed>";
            llvm::errs() << "\n";
          }
        }
      }

    private:
      llvm::SmallVector<SigmaPlacement, 8> Placements;
  };

  extern bool DEBUG;

  class SSIPass : public llvm::FunctionPass
  {
    public:
      static char ID;
      SSIPass() : llvm::FunctionPass(ID) {}
      bool runOnFunction(llvm::Function &F) override;
      void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
      llvm::StringRef getPassName() const override { return "Static Single Information Construction"; }

      SSIFunctionInfo &getInfo() { return Info; }
      const SSIFunctionInfo &getInfo() const { return Info; }

    private:
      void findSigmaSites(llvm::Function &F, llvm::PostDominatorTree &PDT);
      static bool needsSigmaAt(llvm::ArrayRef<llvm::BasicBlock *> Succs, llvm::PostDominatorTree &PDT);

      SSIFunctionInfo Info;
  };
}