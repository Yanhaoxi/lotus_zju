//===- GlobalCleanup.cpp - Cleanup global symbols post-bitcode-link -------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
// ===---------------------------------------------------------------------===//
//
// TPA input file should have no external symbols or aliases. These passes
// internalize (or otherwise remove/resolve) GlobalValues and resolve all
// GlobalAliases.
//
//===----------------------------------------------------------------------===//

#include "Alias/TPA/Transforms/GlobalCleanup.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"

using namespace llvm;

namespace transform {

static bool cleanUpLinkage(GlobalValue *gv) {
  // TODO: handle the rest of the linkage types as necessary without
  // running afoul of the IR verifier or breaking the native link
  switch (gv->getLinkage()) {
  case GlobalValue::ExternalWeakLinkage: {
    Constant *nullRef = Constant::getNullValue(gv->getType());
    gv->replaceAllUsesWith(nullRef);
    gv->eraseFromParent();
    return true;
  }
  case GlobalValue::WeakAnyLinkage: {
    gv->setLinkage(GlobalValue::InternalLinkage);
    return true;
  }
  default:
    // default with fall through to avoid compiler warning
    return false;
  }
  return false;
}

PreservedAnalyses GlobalCleanup::run(Module &M,
                                     ModuleAnalysisManager &analysisManager) {
  bool modified = false;

  if (GlobalVariable *gv = M.getNamedGlobal("llvm.compiler.used")) {
    gv->eraseFromParent();
    modified = true;
  }
  if (GlobalVariable *gv = M.getNamedGlobal("llvm.used")) {
    gv->eraseFromParent();
    modified = true;
  }

  for (auto &gv : M.globals())
    modified |= cleanUpLinkage(&gv);

  for (auto &f : M)
    modified |= cleanUpLinkage(&f);

  return modified ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

PreservedAnalyses ResolveAliases::run(Module &M,
                                      ModuleAnalysisManager &analysisManager) {
  bool modified = false;

  std::vector<GlobalAlias *> aliasToRemove;
  aliasToRemove.reserve(M.alias_size());

  for (auto &alias : M.aliases()) {
    alias.replaceAllUsesWith(alias.getAliasee());
    aliasToRemove.push_back(&alias);
    modified = true;
  }

  for (auto *alias : aliasToRemove)
    alias->eraseFromParent();
  return modified ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

} // namespace transform
