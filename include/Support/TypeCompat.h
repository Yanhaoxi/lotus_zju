#pragma once

#include "Support/LLVMVersion.h"

#include <llvm/IR/DataLayout.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Type.h>

namespace compat {

inline bool isPointerTy(const llvm::Type *T) { return T && T->isPointerTy(); }

inline unsigned getPointerAddressSpace(const llvm::Type *PtrTy) {
  return llvm::cast<llvm::PointerType>(PtrTy)->getAddressSpace();
}

inline unsigned getPointerSizeInBits(const llvm::DataLayout &DL, const llvm::Type *PtrTy) {
  return DL.getPointerTypeSizeInBits(const_cast<llvm::Type *>(PtrTy));
}

// Best-effort: return pointee type when it is known to exist.
// - Non-opaque pointers: returns element type.
// - Opaque pointers: returns nullptr.
inline llvm::Type *tryGetPointerElementType(const llvm::Type *PtrTy) {
  auto *PTy = llvm::dyn_cast<llvm::PointerType>(PtrTy);
  if (!PTy) return nullptr;

#if LOTUS_LLVM_OPAQUE_POINTERS_DEFAULT
  if (PTy->isOpaque()) return nullptr;
  return PTy->getNonOpaquePointerElementType();
#else
  return PTy->getPointerElementType();
#endif
}

} // namespace compat

