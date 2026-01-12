//===- ExpandConstantExpr.cpp - Convert ConstantExprs to Instructions------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass expands out ConstantExprs into Instructions.
//
// Note that this only converts ConstantExprs that are referenced by
// Instructions.  It does not convert ConstantExprs that are used as
// initializers for global variables.
//
// This simplifies the language so that the later analyses do not
// need to handle ConstantExprs when scanning through instructions
//
//===----------------------------------------------------------------------===//

#include "Alias/TPA/Transforms/ExpandConstantExpr.h"

#include "llvm/IR/Constants.h"

#include "Alias/TPA/Transforms/ExpandUtils.h"

using namespace llvm;

namespace transform {

static bool expandInstruction(Instruction *inst);

static Value *expandConstantExpr(Instruction *insertPt, ConstantExpr *expr) {
  Instruction *newInst = expr->getAsInstruction();
  newInst->insertBefore(insertPt);
  newInst->setName("expanded");
  expandInstruction(newInst);
  return newInst;
}

static bool expandInstruction(Instruction *inst) {
  // A landingpad can only accept ConstantExprs, so it should remain
  // unmodified.
  if (isa<LandingPadInst>(inst))
    return false;

  bool modified = false;
  for (unsigned opNum = 0; opNum < inst->getNumOperands(); ++opNum) {
    if (ConstantExpr *expr = dyn_cast<ConstantExpr>(inst->getOperand(opNum))) {
      modified = true;
      Use *user = &inst->getOperandUse(opNum);
      phiSafeReplaceUses(user, expandConstantExpr(phiSafeInsertPt(user), expr));
    }
  }
  return modified;
}

PreservedAnalyses
ExpandConstantExprPass::run(Function &F,
                            FunctionAnalysisManager &analysisManager) {
  bool modified = false;
  for (auto &bb : F) {
    for (auto &inst : bb)
      modified |= expandInstruction(&inst);
  }
  return modified ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

} // namespace transform
