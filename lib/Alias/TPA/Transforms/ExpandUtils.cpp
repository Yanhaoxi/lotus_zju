//===-- ExpandUtils.cpp - Helper functions for expansion passes -----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "Alias/TPA/Transforms/ExpandUtils.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/Module.h"

using namespace llvm;

namespace transform {

Instruction *phiSafeInsertPt(Use *use) {
  Instruction *insertPt = cast<Instruction>(use->getUser());
  if (PHINode *phiNode = dyn_cast<PHINode>(insertPt)) {
    insertPt = phiNode->getIncomingBlock(*use)->getTerminator();
  }
  return insertPt;
}

void phiSafeReplaceUses(Use *use, Value *newVal) {
  if (PHINode *phiNode = dyn_cast<PHINode>(use->getUser())) {
    for (unsigned i = 0; i < phiNode->getNumIncomingValues(); ++i) {
      if (phiNode->getIncomingBlock(i) == phiNode->getIncomingBlock(*use))
        phiNode->setIncomingValue(i, newVal);
    }
  } else {
    use->getUser()->replaceUsesOfWith(use->get(), newVal);
  }
}

Function *recreateFunction(Function *func, FunctionType *newType) {
  Function *newFunc = Function::Create(newType, func->getLinkage());
  newFunc->copyAttributesFrom(func);
  func->getParent()->getFunctionList().insert(
      func->getParent()->getFunctionList().begin(), newFunc);
  newFunc->takeName(func);
  newFunc->getBasicBlockList().splice(newFunc->begin(),
                                      func->getBasicBlockList());
  func->replaceAllUsesWith(ConstantExpr::getBitCast(
      newFunc, func->getFunctionType()->getPointerTo()));
  return newFunc;
}

Instruction *copyDebug(Instruction *newInst, Instruction *original) {
  newInst->setDebugLoc(original->getDebugLoc());
  return newInst;
}

} // namespace transform
