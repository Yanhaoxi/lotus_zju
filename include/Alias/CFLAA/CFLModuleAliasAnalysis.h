//===- CFLModuleAliasAnalysis.h - Module-scope CFL AA ----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
/// \file
/// A module-level wrapper that builds a single CFL graph across the whole
/// module and answers alias queries without the per-function restriction of the
/// existing CFL AA implementations.
//===----------------------------------------------------------------------===//

#ifndef LLVM_ANALYSIS_CFLMODULEALIASANALYSIS_H
#define LLVM_ANALYSIS_CFLMODULEALIASANALYSIS_H

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/IR/Module.h"
#include "Alias/CFLAA/CFLGraph.h"
#include "Alias/CFLAA/StratifiedSets.h"

namespace llvm {
namespace cflaa {

enum class CFLModuleAAAlgorithm {
  Steens,
  Anders
};

class CFLModuleAAResult {
public:
  CFLModuleAAResult() = default;
  explicit CFLModuleAAResult(CFLModuleAAAlgorithm Algo) : Algorithm(Algo) {}
  ~CFLModuleAAResult();

  CFLModuleAAResult(const CFLModuleAAResult &) = delete;
  CFLModuleAAResult &operator=(const CFLModuleAAResult &) = delete;
  CFLModuleAAResult(CFLModuleAAResult &&) = default;
  CFLModuleAAResult &operator=(CFLModuleAAResult &&) = default;

  AliasResult alias(const Value *V1, const Value *V2, AAQueryInfo &AAQI) const;

  bool invalidate(Module &, const PreservedAnalyses &,
                  ModuleAnalysisManager::Invalidator &) {
    return false;
  }

private:
  friend class CFLModuleAA;
  CFLModuleAAAlgorithm Algorithm = CFLModuleAAAlgorithm::Steens;
  
  // Steens-style result
  StratifiedSets<InstantiatedValue> Sets;
  
  // Anders-style result (only used when Algorithm == Anders)
  // Opaque pointer - actual type defined in .cpp
  void *AndersData = nullptr;
};

class CFLModuleAA : public ModulePass {
public:
  static char ID;
  CFLModuleAA();
  explicit CFLModuleAA(CFLModuleAAAlgorithm Algo);

  bool runOnModule(Module &M) override;
  void getAnalysisUsage(AnalysisUsage &AU) const override;

  CFLModuleAAResult &getResult() { return Result; }

private:
  CFLModuleAAResult Result;
};

} // end namespace cflaa
} // end namespace llvm

#endif // LLVM_ANALYSIS_CFLMODULEALIASANALYSIS_H
