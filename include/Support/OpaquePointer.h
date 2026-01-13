#pragma once

#include "Support/LLVMVersion.h"

#include <llvm/Config/llvm-config.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>

namespace lotus {
namespace compat {

// Helper to handle opaque pointers (LLVM 15+)
// In opaque pointers, PointerType no longer carries the element type.
// We must get the type from the value context or explicit type arguments.

inline bool isOpaquePointerType(const llvm::Type *T) {
#if LOTUS_LLVM_OPAQUE_POINTERS_DEFAULT
    if (auto *PTy = llvm::dyn_cast<llvm::PointerType>(T)) return PTy->isOpaque();
    return false;
#else
    (void)T;
    return false;
#endif
}

// Best-effort "try" API:
// - returns the element type if it exists
// - returns nullptr for opaque pointers (or non-pointers)
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

// Legacy shim (discouraged): if the element type is not available, fall back
// to i8 ("byte") so older analyses can keep going conservatively.
inline llvm::Type *getPointerElementTypeOrI8(const llvm::Type *PtrTy) {
    if (auto *Elt = tryGetPointerElementType(PtrTy)) return Elt;
    return llvm::Type::getInt8Ty(PtrTy->getContext());
}

inline llvm::Type *getPointerElementType(const llvm::Value *V) {
    return getPointerElementTypeOrI8(V->getType());
}

// Safer alternative: Get element type from Load/Store/GEP instructions which explicitly have it
inline llvm::Type *getLoadStoreType(const llvm::Instruction *I) {
    if (const auto *LI = llvm::dyn_cast<llvm::LoadInst>(I)) {
        return LI->getType();
    }
    if (const auto *SI = llvm::dyn_cast<llvm::StoreInst>(I)) {
        return SI->getValueOperand()->getType();
    }
    return nullptr;
}

// For GEPs, the source element type is explicit in newer LLVMs
inline llvm::Type *getGEPSourceElementType(const llvm::GetElementPtrInst *GEP) {
#if LOTUS_LLVM_VERSION_AT_LEAST(14, 0)
    return GEP->getSourceElementType();
#else
    return GEP->getPointerOperandType()->getPointerElementType();
#endif
}

} // namespace compat
} // namespace lotus
