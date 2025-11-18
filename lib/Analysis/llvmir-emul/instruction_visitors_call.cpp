/**
 * @file src/llvmir-emul/instruction_visitors_call.cpp
 * @brief Call instruction visitor implementations.
 * @copyright (c) 2017 Avast Software, licensed under the MIT license
 */

#include "Analysis/llvmir-emul/llvmir_emul.h"
#include <llvm/IR/Instructions.h>
#include <llvm/CodeGen/IntrinsicLowering.h>

using namespace llvm;

namespace retdec {
namespace llvmir_emul {
 void LlvmIrEmulator::visitCallInst(llvm::CallInst& I)
 {
     LocalExecutionContext& ec = _ecStack.back();
 
     auto* cf = I.getCalledFunction();
     if (cf && cf->isDeclaration() && cf->isIntrinsic() &&
             !(cf->getIntrinsicID() == Intrinsic::bitreverse
             || cf->getIntrinsicID() == Intrinsic::maxnum
             || cf->getIntrinsicID() == Intrinsic::minnum
             || cf->getIntrinsicID() == Intrinsic::fabs)) // can not lower those functions
     {
         assert(cf->getIntrinsicID() != Intrinsic::vastart
                 && cf->getIntrinsicID() != Intrinsic::vaend
                 && cf->getIntrinsicID() != Intrinsic::vacopy);
 
         BasicBlock::iterator me(&I);
         BasicBlock *Parent = I.getParent();
         bool atBegin(Parent->begin() == me);
         if (!atBegin)
         {
             --me;
         }
         IL->LowerIntrinsicCall(cast<CallInst>(&I));
 
         // Restore the CurInst pointer to the first instruction newly inserted,
         // if any.
         if (atBegin)
         {
             ec.curInst = Parent->begin();
         }
         else
         {
             ec.curInst = me;
             ++ec.curInst;
         }
 
         return;
     }
 
    CallEntry ce;
    ce.calledValue = I.getCalledOperand();
 
     for (auto aIt = I.arg_begin(), eIt = I.arg_end(); aIt != eIt; ++aIt)
     {
         Value* val = *aIt;
         ce.calledArguments.push_back(_globalEc.getOperandValue(val, ec));
     }
 
     _calls.push_back(ce);
 }
 
 void LlvmIrEmulator::visitInvokeInst(llvm::InvokeInst& I)
 {
     assert(false && "InvokeInst not implemented.");
     throw LlvmIrEmulatorError("InvokeInst not implemented.");
 }

} // llvmir_emul
} // retdec
