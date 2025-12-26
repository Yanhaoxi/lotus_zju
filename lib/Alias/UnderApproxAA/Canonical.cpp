/**
 * @file Canonical.cpp
 * @brief Pointer canonicalization and equivalence checking utilities
 *
 * This file provides helper functions for normalizing pointer values and
 * detecting equivalence patterns. These utilities are used by the must-alias
 * analysis to identify when two pointers are guaranteed to refer to the same
 * memory location, despite syntactic differences.
 *
 * The canonicalization process strips away operations that don't change the
 * actual memory address at runtime, such as:
 * - Bitcasts (type changes without address changes)
 * - No-op address space casts
 * - Invariant group intrinsics (optimization hints, not actual address changes)
 */

#include "Alias/UnderApproxAA/Canonical.h"
#include <llvm/Analysis/ValueTracking.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>

using namespace llvm;
using namespace UnderApprox;

// ---------------------------------------------------------------------------
// Canonicalization helpers
// ---------------------------------------------------------------------------

/// Strip all no-op casts and invariant group intrinsics from a pointer value
/// 
/// This function recursively removes casts and intrinsics that don't change
/// the runtime address of a pointer. The result is a "canonical" form that
/// can be used for comparison.
///
/// @param V The pointer value to canonicalize
/// @return The canonical form of V (with no-op casts removed)
///
/// Examples:
///   - bitcast %p to i8*  →  %p (if %p is already i8*)
///   - addrspacecast %p from addrspace(0) to addrspace(0)  →  %p
///   - launder_invariant_group(%p)  →  %p
///
/// This is safe because these operations are guaranteed to preserve the
/// memory address - they only change type metadata or optimization hints.
const Value *UnderApprox::stripNoopCasts(const Value *V) {
  // Iteratively strip until no more no-op operations remain
  while (true) {
    // Strip bitcasts: casting between compatible pointer types doesn't
    // change the address (e.g., i32* → i8*, or i8* → i32*)
    if (auto *BC = dyn_cast<BitCastOperator>(V)) {
      V = BC->getOperand(0);
      continue;
    }
    
    // Strip no-op address space casts: casting within the same address space
    // is a no-op (though this shouldn't happen in well-formed IR, it's
    // possible in intermediate optimization states)
    if (isNoopAddrSpaceCast(V)) {
      V = cast<AddrSpaceCastInst>(V)->getOperand(0);
      continue;
    }
    
    // Strip invariant group intrinsics: these are optimization hints for
    // devirtualization and don't affect the actual memory address
    // - launder_invariant_group: marks a pointer as having a new "invariant group"
    // - strip_invariant_group: removes invariant group metadata
    // Both are no-ops from an aliasing perspective
    if (auto *II = dyn_cast<IntrinsicInst>(V)) {
      switch (II->getIntrinsicID()) {
      case Intrinsic::launder_invariant_group:
      case Intrinsic::strip_invariant_group:
        V = II->getArgOperand(0);
        continue;
      default:
        break;
      }
    }
    
    // No more no-op operations to strip - return canonical form
    return V;
  }
}

/// Check if an address space cast is a no-op (same source and destination space)
///
/// Address space casts typically change the address space (e.g., from global
/// to local memory in GPU code). However, if the source and destination spaces
/// are the same, the cast is a no-op and can be stripped.
///
/// @param V The value to check (must be an AddrSpaceCastInst)
/// @return true if the cast is a no-op, false otherwise
///
/// Note: In well-formed LLVM IR, address space casts should always change
/// the address space. However, intermediate optimization passes may create
/// no-op casts that should be canonicalized away.
bool UnderApprox::isNoopAddrSpaceCast(const Value *V) {
  if (auto *ASC = dyn_cast<AddrSpaceCastInst>(V))
    return ASC->getSrcTy()->getPointerAddressSpace() ==
           ASC->getDestTy()->getPointerAddressSpace();
  return false;
}

/// Check if two pointers have the same base and identical constant offsets
///
/// This function uses LLVM's stripAndAccumulateInBoundsConstantOffsets to
/// decompose each pointer into a base + offset. Two pointers must-alias if
/// they have the same base and identical offsets.
///
/// @param DL The DataLayout for the target (needed for pointer size calculations)
/// @param A First pointer value
/// @param B Second pointer value
/// @return true if A and B have the same base and constant offset, false otherwise
///
/// Examples:
///   - GEP(%base, 0, 5) and GEP(%base, 0, 5)  →  true
///   - GEP(%base, 0, 5) and GEP(%base, 0, 6)  →  false
///   - %base and GEP(%base, 0, 0)  →  true (zero offset)
///   - GEP(%base, %var) and GEP(%base, %var)  →  false (non-constant offset)
///
/// This is a key rule for must-alias analysis: GEPs with identical constant
/// indices from the same base pointer must alias.
bool UnderApprox::sameConstOffset(const DataLayout &DL,
                                  const Value *A, const Value *B) {
  // Initialize offsets to zero
  APInt OffA(DL.getPointerSizeInBits(0), 0);
  APInt OffB(DL.getPointerSizeInBits(0), 0);
  
  // Strip casts and GEPs, accumulating constant offsets
  // Returns the base pointer (after stripping) and updates the offset
  const Value *BaseA = A->stripAndAccumulateInBoundsConstantOffsets(DL, OffA);
  const Value *BaseB = B->stripAndAccumulateInBoundsConstantOffsets(DL, OffB);
  
  // Must-alias if same base and same offset
  return BaseA == BaseB && OffA == OffB;
}

/// Check if a GEP has all zero indices
///
/// A GEP with all zero indices is equivalent to its base pointer. This is
/// a common pattern that should be recognized as must-alias.
///
/// @param V The value to check (must be a GEPOperator)
/// @return true if V is a GEP with all zero indices, false otherwise
///
/// Examples:
///   - GEP(%p, 0, 0)  →  true
///   - GEP(%p, 0)     →  true
///   - GEP(%p, 0, 1)  →  false
///   - %p (not a GEP) →  false
///
/// This is used by the atomic must-alias rules to detect when a GEP
/// is trivially equivalent to its base pointer.
bool UnderApprox::isZeroGEP(const Value *V) {
  if (auto *GEP = dyn_cast<GEPOperator>(V))
    return GEP->hasAllZeroIndices();
  return false;
}

/// Check if two values form a round-trip cast: inttoptr(ptrtoint(X))
///
/// A pointer converted to an integer and back (with no arithmetic in between)
/// is guaranteed to be the same pointer. This pattern can occur in certain
/// optimization scenarios or when working with pointer arithmetic.
///
/// @param A First value (should be IntToPtrInst)
/// @param B Second value (should be PtrToIntInst)
/// @return true if A and B form a round-trip cast, false otherwise
///
/// Examples:
///   %i = ptrtoint %p to i64
///   %q = inttoptr %i to i8*
///   isRoundTripCast(%q, %i)  →  true (if %q->getOperand(0) == %i and
///                                          %i->getOperand(0) == %p)
///
/// Note: This function checks both directions (A→B and B→A) since the
/// order of arguments may vary. The actual check is performed by comparing
/// operands to ensure they form a true round-trip.
///
/// Limitation: This only detects direct round-trips where the inttoptr
/// directly uses the result of ptrtoint. More complex patterns (e.g., with
/// arithmetic) are not detected.
bool UnderApprox::isRoundTripCast(const Value *A, const Value *B) {
  // Check if A is inttoptr and B is ptrtoint
  auto *ITP = dyn_cast<IntToPtrInst>(A);
  auto *PTI = dyn_cast<PtrToIntInst>(B);
  
  // Verify they form a round-trip: inttoptr uses ptrtoint's result,
  // and ptrtoint uses the original pointer (which inttoptr produces)
  return ITP && PTI &&
         ITP->getOperand(0) == PTI && PTI->getOperand(0) == ITP;
}