#pragma once

#include "Support/LLVMVersion.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/ErrorHandling.h>

namespace compat {

// IRBuilder helpers that smooth over API differences and (most importantly)
// make opaque-pointer builds explicit about element types.
//
// Design choice:
// - Under opaque pointers (LLVM 15+ default), we *refuse* to infer element types
//   from pointer types because doing so is incorrect. Callers must pass the
//   element type explicitly.

template <typename BuilderT>
inline llvm::LoadInst *CreateLoad(BuilderT &B, llvm::Type *ElemTy, llvm::Value *Ptr,
                                  const llvm::Twine &Name = "") {
  return B.CreateLoad(ElemTy, Ptr, Name);
}

template <typename BuilderT>
inline llvm::LoadInst *CreateLoad(BuilderT &B, llvm::Value *Ptr, const llvm::Twine &Name = "") {
#if LOTUS_LLVM_OPAQUE_POINTERS_DEFAULT
  llvm_unreachable("CreateLoad(ptr) requires explicit element type under opaque pointers");
#else
  auto *PTy = llvm::cast<llvm::PointerType>(Ptr->getType());
  return B.CreateLoad(PTy->getPointerElementType(), Ptr, Name);
#endif
}

template <typename BuilderT>
inline llvm::Value *CreateGEP(BuilderT &B, llvm::Type *SourceElemTy, llvm::Value *Ptr,
                              llvm::ArrayRef<llvm::Value *> IdxList,
                              const llvm::Twine &Name = "") {
  return B.CreateGEP(SourceElemTy, Ptr, IdxList, Name);
}

template <typename BuilderT>
inline llvm::Value *CreateGEP(BuilderT &B, llvm::Value *Ptr, llvm::ArrayRef<llvm::Value *> IdxList,
                              const llvm::Twine &Name = "") {
#if LOTUS_LLVM_OPAQUE_POINTERS_DEFAULT
  llvm_unreachable("CreateGEP(ptr, idx) requires explicit source element type under opaque pointers");
#else
  auto *PTy = llvm::cast<llvm::PointerType>(Ptr->getType());
  return B.CreateGEP(PTy->getPointerElementType(), Ptr, IdxList, Name);
#endif
}

template <typename BuilderT>
inline llvm::Value *CreateInBoundsGEP(BuilderT &B, llvm::Type *SourceElemTy, llvm::Value *Ptr,
                                      llvm::ArrayRef<llvm::Value *> IdxList,
                                      const llvm::Twine &Name = "") {
  return B.CreateInBoundsGEP(SourceElemTy, Ptr, IdxList, Name);
}

template <typename BuilderT>
inline llvm::Value *CreateInBoundsGEP(BuilderT &B, llvm::Value *Ptr,
                                      llvm::ArrayRef<llvm::Value *> IdxList,
                                      const llvm::Twine &Name = "") {
#if LOTUS_LLVM_OPAQUE_POINTERS_DEFAULT
  llvm_unreachable("CreateInBoundsGEP(ptr, idx) requires explicit source element type under opaque pointers");
#else
  auto *PTy = llvm::cast<llvm::PointerType>(Ptr->getType());
  return B.CreateInBoundsGEP(PTy->getPointerElementType(), Ptr, IdxList, Name);
#endif
}

} // namespace compat

