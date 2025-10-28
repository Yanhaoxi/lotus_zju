/*
 * LotusAA - Pointer Operation Transfer Functions
 * 
 * Transfer functions for pointer manipulation: PHI, select, GEP, casts.
 * Also contains the main processBasePointer() dispatcher.
 */

#include "Alias/LotusAA/Engine/IntraProceduralAnalysis.h"

#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Operator.h>

using namespace llvm;

PTResult *IntraLotusAA::processPhi(PHINode *phi) {
  PTResult *phi_pts = findPTResult(phi, true);

  for (unsigned i = 0; i < phi->getNumIncomingValues(); i++) {
    Value *val_i = phi->getIncomingValue(i);
    PTResult *in_pts = processBasePointer(val_i);
    assert(in_pts && "PHI incoming value not processed");
    phi_pts->add_derived_target(in_pts, 0);
  }

  PTResultIterator iter(phi_pts, this);
  return phi_pts;
}

PTResult *IntraLotusAA::processSelect(SelectInst *select) {
  if (!select->getType()->isPointerTy())
    return nullptr;

  Value *true_val = select->getTrueValue();
  Value *false_val = select->getFalseValue();

  PTResult *pts_true = processBasePointer(true_val);
  PTResult *pts_false = processBasePointer(false_val);

  PTResult *select_pts = findPTResult(select, true);
  select_pts->add_derived_target(pts_true, 0);
  select_pts->add_derived_target(pts_false, 0);

  PTResultIterator iter(select_pts, this);
  return select_pts;
}

PTResult *IntraLotusAA::processGepBitcast(Value *ptr) {
  // Track pointer through GEP/bitcast operations
  int64_t offset = 0;
  Value *base_ptr = ptr;

  // For GEP, extract base pointer
  // Note: Offset tracking is intentionally simplified to 0
  // Field-sensitivity is handled through ObjectLocator field tracking,
  // not through offset arithmetic in points-to results
  if (GEPOperator *gep = dyn_cast<GEPOperator>(ptr)) {
    base_ptr = gep->getPointerOperand();
    offset = 0;  // Field offsets handled by ObjectLocator
  } else if (BitCastInst *bc = dyn_cast<BitCastInst>(ptr)) {
    base_ptr = bc->getOperand(0);
    offset = 0;
  }

  if (base_ptr == ptr) {
    return addPointsTo(ptr, newObject(ptr, MemObject::CONCRETE), 0);
  }

  PTResult *pts = processBasePointer(base_ptr);
  PTResult *ret = derivePtsFrom(ptr, pts, offset);
  PTResultIterator iter(ret, this);
  return ret;
}

PTResult *IntraLotusAA::processCast(CastInst *cast) {
  Value *base_ptr = cast->getOperand(0);
  PTResult *pts = processBasePointer(base_ptr);
  PTResult *ret = derivePtsFrom(cast, pts, 0);
  PTResultIterator iter(ret, this);
  return ret;
}

PTResult *IntraLotusAA::processBasePointer(Value *base_ptr) {
  PTResult *res = findPTResult(base_ptr);
  if (res)
    return res;

  if (isa<GEPOperator>(base_ptr) || isa<BitCastInst>(base_ptr)) {
    res = processGepBitcast(base_ptr);
  } else if (CastInst *cast = dyn_cast<CastInst>(base_ptr)) {
    res = processCast(cast);
  } else if (Argument *arg = dyn_cast<Argument>(base_ptr)) {
    res = processArg(arg);
  } else if (ConstantPointerNull *cnull = dyn_cast<ConstantPointerNull>(base_ptr)) {
    res = processNullptr(cnull);
  } else if (GlobalValue *gv = dyn_cast<GlobalValue>(base_ptr)) {
    res = processGlobal(gv);
  } else if (ConstantExpr *ce = dyn_cast<ConstantExpr>(base_ptr)) {
    if (ce->getOpcode() == Instruction::BitCast || 
        ce->getOpcode() == Instruction::GetElementPtr)
      res = processGepBitcast(base_ptr);
  } else if (!base_ptr->getType()->isPointerTy()) {
    res = processNonPointer(base_ptr);
  }

  if (!res)
    res = processUnknown(base_ptr);

  return res;
}

