/*
 * LotusAA - Memory Operation Transfer Functions
 * 
 * Transfer functions for load and store instructions.
 * Handles points-to propagation through memory reads/writes.
 */

#include "Alias/LotusAA/Engine/IntraProceduralAnalysis.h"

#include <llvm/IR/Instructions.h>

using namespace llvm;

void IntraLotusAA::processLoad(LoadInst *load_inst) {
  Value *load_ptr = load_inst->getPointerOperand();
  processBasePointer(load_ptr);

  if (!load_inst->getType()->isPointerTy())
    return;

  mem_value_t result;
  loadPtrAt(load_ptr, load_inst, result, true);

  PTResult *load_pts = findPTResult(load_inst, true);

  for (auto &load_pair : result) {
    Value *fld_val = load_pair.val;

    if (fld_val == LocValue::FREE_VARIABLE ||
        fld_val == LocValue::UNDEF_VALUE ||
        fld_val == LocValue::SUMMARY_VALUE)
      continue;

    PTResult *fld_pts = processBasePointer(fld_val);
    load_pts->add_derived_target(fld_pts, 0);
  }
  
  PTResultIterator iter(load_pts, this);
}

void IntraLotusAA::processStore(StoreInst *store) {
  Value *ptr = store->getPointerOperand();
  Value *store_value = store->getValueOperand();
  PTResult *res = processBasePointer(ptr);
  assert(res && "Store pointer not processed");

  PTResultIterator iter(res, this);

  for (auto loc : iter) {
    MemObject *obj = loc->getObj();
    if (obj->isNull() || obj->isUnknown())
      continue;

    loc->storeValue(store_value, store, 0);
  }

  if (store_value->getType()->isPointerTy()) {
    processBasePointer(store_value);
  }
}

