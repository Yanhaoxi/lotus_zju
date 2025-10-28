/*
 * LotusAA - Basic Value Transfer Functions
 * 
 * Transfer functions for basic pointer sources: alloca, arguments, globals, constants.
 */

#include "Alias/LotusAA/Engine/IntraProceduralAnalysis.h"

#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Argument.h>
#include <llvm/IR/GlobalValue.h>

using namespace llvm;

PTResult *IntraLotusAA::processAlloca(AllocaInst *alloca) {
  return addPointsTo(alloca, newObject(alloca), 0);
}

PTResult *IntraLotusAA::processArg(Argument *arg) {
  MemObject::ObjKind kind = func_pseudo_ret_cache.count(arg) 
                             ? MemObject::CONCRETE 
                             : MemObject::SYMBOLIC;
  return addPointsTo(arg, newObject(arg, kind), 0);
}

PTResult *IntraLotusAA::processGlobal(GlobalValue *global) {
  return addPointsTo(global, newObject(global, MemObject::SYMBOLIC), 0);
}

PTResult *IntraLotusAA::processNullptr(ConstantPointerNull *null_ptr) {
  return assignPts(null_ptr, NullPTS);
}

PTResult *IntraLotusAA::processNonPointer(Value *non_pointer_val) {
  if (CastInst *cast = dyn_cast<CastInst>(non_pointer_val)) {
    Value *src = cast->getOperand(0);
    if (src->getType()->isPointerTy()) {
      PTResult *src_res = processBasePointer(src);
      return derivePtsFrom(non_pointer_val, src_res, 0);
    }
  }
  return addPointsTo(non_pointer_val, newObject(non_pointer_val), 0);
}

PTResult *IntraLotusAA::processUnknown(Value *unknown_val) {
  return addPointsTo(unknown_val, MemObject::UnknownObj, 0);
}

