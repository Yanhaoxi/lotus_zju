// Implementation of PointerLayoutAnalysis.
//
// Identifies all offsets within a type that contain pointers.
//
// Key Feature: Layout Propagation via Casts.
// Since pointers can be cast between different struct types (especially in C),
// we must ensure that the pointer analysis "sees" pointers even if they are accessed
// through a casted type.
//
// Algorithm:
// 1. Build initial layout: recursively scan types to find pointer fields.
// 2. Propagate layouts: Using the CastMap (from StructCastAnalysis), merge layout information.
//    If StructA is cast to StructB, then StructA effectively "has" pointers where StructB does.
//    (Conservative approach to handle unsafe casts).

#include "Alias/TPA/PointerAnalysis/FrontEnd/Type/PointerLayoutAnalysis.h"

#include "Alias/TPA/PointerAnalysis/FrontEnd/Type/CastMap.h"
#include "Alias/TPA/PointerAnalysis/FrontEnd/Type/TypeSet.h"
#include "Alias/TPA/PointerAnalysis/MemoryModel/Type/PointerLayout.h"

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Type.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;

namespace tpa {

namespace {

class PtrLayoutMapBuilder {
private:
  const TypeSet &typeSet;
  PointerLayoutMap &ptrLayoutMap;

  void insertMap(const Type *, const PointerLayout *);

  const PointerLayout *processStructType(StructType *);
  const PointerLayout *processArrayType(ArrayType *);
  const PointerLayout *processPointerType(Type *);
  const PointerLayout *processNonPointerType(Type *);
  const PointerLayout *processType(Type *);

public:
  PtrLayoutMapBuilder(const TypeSet &t, PointerLayoutMap &p)
      : typeSet(t), ptrLayoutMap(p) {}

  void buildPtrLayoutMap();
};

void PtrLayoutMapBuilder::insertMap(const Type *type,
                                    const PointerLayout *layout) {
  ptrLayoutMap.insert(type, layout);
}

const PointerLayout *
PtrLayoutMapBuilder::processStructType(StructType *stType) {
  // We know nothing about opaque type. Conservatively treat it as a non-pointer
  // blob.
  if (stType->isOpaque()) {
    const auto *layout = PointerLayout::getEmptyLayout();
    insertMap(stType, layout);
    return layout;
  }

  util::VectorSet<size_t> ptrOffsets;

  const auto *structLayout = typeSet.getDataLayout().getStructLayout(stType);
  for (unsigned i = 0, e = stType->getNumElements(); i != e; ++i) {
    auto offset = structLayout->getElementOffset(i);
    auto *subType = stType->getElementType(i);
    const auto *subLayout = processType(subType);

    // Add offsets from sub-type, shifted by the field offset
    for (auto subOffset : *subLayout)
      ptrOffsets.insert(subOffset + offset);
  }

  const auto *stPtrLayout = PointerLayout::getLayout(std::move(ptrOffsets));
  insertMap(stType, stPtrLayout);
  return stPtrLayout;
}

const PointerLayout *
PtrLayoutMapBuilder::processArrayType(ArrayType *arrayType) {
  // For arrays, we just use the element layout.
  // NOTE: This assumes array accesses are collapsed to element 0.
  const auto *layout = processType(arrayType->getElementType());
  insertMap(arrayType, layout);
  return layout;
}

const PointerLayout *PtrLayoutMapBuilder::processPointerType(Type *ptrType) {
  const auto *layout = PointerLayout::getSinglePointerLayout();
  insertMap(ptrType, layout);
  return layout;
}

const PointerLayout *
PtrLayoutMapBuilder::processNonPointerType(Type *nonPtrType) {
  const auto *layout = PointerLayout::getEmptyLayout();
  insertMap(nonPtrType, layout);
  return layout;
}

const PointerLayout *PtrLayoutMapBuilder::processType(Type *type) {
  const auto *layout = ptrLayoutMap.lookup(type);
  if (layout != nullptr)
    return layout;

  if (auto *stType = dyn_cast<StructType>(type))
    return processStructType(stType);
  else if (auto *arrayType = dyn_cast<ArrayType>(type))
    return processArrayType(arrayType);
  else if (type->isPointerTy() || type->isFunctionTy())
    return processPointerType(type);
  else
    return processNonPointerType(type);
}

void PtrLayoutMapBuilder::buildPtrLayoutMap() {
  for (auto *type : typeSet)
    processType(type);
}

// Propagates pointer layout information across bitcasts.
class PtrLayoutMapPropagator {
private:
  const CastMap &castMap;
  PointerLayoutMap &ptrLayoutMap;

public:
  PtrLayoutMapPropagator(const CastMap &c, PointerLayoutMap &p)
      : castMap(c), ptrLayoutMap(p) {}

  void propagatePtrLayoutMap();
};

void PtrLayoutMapPropagator::propagatePtrLayoutMap() {
  // For every cast mapping LHS -> {RHS1, RHS2...}
  for (auto const &mapping : castMap) {
    auto *lhs = mapping.first;
    const auto *dstLayout = ptrLayoutMap.lookup(lhs);
    assert(dstLayout != nullptr && "Cannot find ptrLayout for lhs type");

    // Merge layout of RHS into LHS.
    // Logic: If LHS is cast to RHS, then memory at LHS might be interpreted as RHS.
    // So if RHS has a pointer at offset X, LHS should also be considered to potentially
    // have a pointer at offset X to be safe.
    for (auto *rhs : mapping.second) {
      const auto *srcLayout = ptrLayoutMap.lookup(rhs);
      assert(srcLayout != nullptr && "Cannot find ptrLayout for src type");
      dstLayout = PointerLayout::merge(dstLayout, srcLayout);
    }
    ptrLayoutMap.insert(lhs, dstLayout);
  }
}

} // namespace

PointerLayoutMap PointerLayoutAnalysis::runOnTypes(const TypeSet &typeSet) {
  PointerLayoutMap ptrLayoutMap;

  // Phase 1: Structural analysis
  PtrLayoutMapBuilder(typeSet, ptrLayoutMap).buildPtrLayoutMap();

  // Phase 2: Propagation via casts
  PtrLayoutMapPropagator(castMap, ptrLayoutMap).propagatePtrLayoutMap();

  return ptrLayoutMap;
}

} // namespace tpa
