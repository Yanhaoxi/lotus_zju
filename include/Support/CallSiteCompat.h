#pragma once

#include "Support/LLVMVersion.h"

#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Value.h>

namespace lotus::compat {

// A small, non-owning wrapper to unify CallSite-like usage on LLVM 12+.
// This intentionally does NOT try to emulate all of legacy llvm::CallSite; it
// just covers the handful of accessors most projects typically need.
class CallSiteRef {
  llvm::CallBase *CB{nullptr};

public:
  CallSiteRef() = default;
  explicit CallSiteRef(llvm::CallBase *CB_) : CB(CB_) {}
  explicit CallSiteRef(llvm::Instruction *I) : CB(llvm::dyn_cast_or_null<llvm::CallBase>(I)) {}
  explicit CallSiteRef(llvm::Value *V) : CB(llvm::dyn_cast_or_null<llvm::CallBase>(V)) {}

  explicit operator bool() const { return CB != nullptr; }
  llvm::CallBase *get() const { return CB; }

  llvm::Value *getCalledOperand() const { return CB ? CB->getCalledOperand() : nullptr; }
  llvm::Function *getCalledFunction() const { return CB ? CB->getCalledFunction() : nullptr; }

  unsigned arg_size() const { return CB ? CB->arg_size() : 0; }
  llvm::Value *getArgOperand(unsigned i) const { return CB ? CB->getArgOperand(i) : nullptr; }

  llvm::Type *getType() const { return CB ? CB->getType() : nullptr; }
  llvm::FunctionType *getFunctionType() const { return CB ? CB->getFunctionType() : nullptr; }

  bool isCall() const { return CB && llvm::isa<llvm::CallInst>(CB); }
  bool isInvoke() const { return CB && llvm::isa<llvm::InvokeInst>(CB); }
};

} // namespace lotus::compat

