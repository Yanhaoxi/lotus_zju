#include "Alias/TPA/PointerAnalysis/Program/SemiSparseProgram.h"

#include <llvm/IR/Module.h>

using namespace llvm;

namespace tpa {

SemiSparseProgram::SemiSparseProgram(const llvm::Module &m) : module(m) {
  for (auto const &f : module) {
    if (f.hasAddressTaken())
      addrTakenFuncList.push_back(&f);
  }
}

CFG &SemiSparseProgram::getOrCreateCFGForFunction(const llvm::Function &f) {
  auto itr = cfgMap.find(&f);
  if (itr == cfgMap.end())
    itr = cfgMap.emplace(&f, f).first;
  return itr->second;
}
CFG &SemiSparseProgram::getOrCreateCFGForFunction(
    const llvm::Function &f) const {
  return const_cast<SemiSparseProgram *>(this)->getOrCreateCFGForFunction(f);
}
const CFG *SemiSparseProgram::getCFGForFunction(const llvm::Function &f) const {
  auto itr = cfgMap.find(&f);
  if (itr == cfgMap.end())
    return nullptr;
  else
    return &itr->second;
}
const CFG *SemiSparseProgram::getEntryCFG() const {
  // Prefer the usual whole-program entry.
  if (auto *mainFunc = module.getFunction("main"))
    return &getOrCreateCFGForFunction(*mainFunc);

  // Some inputs (e.g., shared libraries) have no `main`. Pick a best-effort
  // entry to avoid crashing the analysis pipeline.
  for (auto const &f : module) {
    if (f.isDeclaration())
      continue;
    if (f.isIntrinsic())
      continue;
    return &getOrCreateCFGForFunction(f);
  }

  // No suitable entry.
  return nullptr;
}

} // namespace tpa
