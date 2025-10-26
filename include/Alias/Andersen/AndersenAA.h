#ifndef TCFS_ANDERSEN_AA_H
#define TCFS_ANDERSEN_AA_H

#include "Alias/Andersen/Andersen.h"

#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/IR/PassManager.h>

class AndersenAAResult : public llvm::AAResultBase<AndersenAAResult> {
private:
  friend llvm::AAResultBase<AndersenAAResult>;

  Andersen anders;
  llvm::AliasResult andersenAlias(const llvm::Value *, const llvm::Value *);

public:
  AndersenAAResult(const llvm::Module &);

  llvm::AliasResult alias(const llvm::MemoryLocation &,
                          const llvm::MemoryLocation &);
  bool pointsToConstantMemory(const llvm::MemoryLocation &, bool);
  
  // Public method to access points-to information
  bool getPointsToSet(const llvm::Value *Ptr, std::vector<const llvm::Value*> &PtsSet) const {
    return anders.getPointsToSet(Ptr, PtsSet);
  }
};

// New Pass Manager version
class AndersenAA : public llvm::AnalysisInfoMixin<AndersenAA> {
  friend llvm::AnalysisInfoMixin<AndersenAA>;
  static llvm::AnalysisKey Key;

public:
  using Result = AndersenAAResult;
  
  AndersenAAResult run(llvm::Module &M, llvm::ModuleAnalysisManager &MAM);
};

#endif
