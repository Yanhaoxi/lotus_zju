/**
 * @file src/llvmir-emul/instruction_visitors_terminator.cpp
 * @brief Terminator instruction visitor implementations.
 * @copyright (c) 2017 Avast Software, licensed under the MIT license
 */

#include "Analysis/llvmir-emul/llvmir_emul.h"
#include <llvm/IR/Instructions.h>
#include <cstring>

using namespace llvm;

namespace retdec {
namespace llvmir_emul {
 void LlvmIrEmulator::popStackAndReturnValueToCaller(
         llvm::Type* retT,
         llvm::GenericValue res)
 {
     _ecStackRetired.emplace_back(_ecStack.back());
     _ecStack.pop_back();
 
     // Finished main. Put result into exit code...
     //
     if (_ecStack.empty())
     {
         if (retT && !retT->isVoidTy())
         {
             _exitValue = res;
         }
         else
         {
             // Matula: This memset is ok.
             memset(&_exitValue.Untyped, 0, sizeof(_exitValue.Untyped));
         }
     }
     // If we have a previous stack frame, and we have a previous call,
     // fill in the return value...
     //
     else
     {
        LocalExecutionContext& callingEc = _ecStack.back();
        if (CallBase* CB = callingEc.caller)
        {
            // Save result...
            if (!CB->getType()->isVoidTy())
            {
                _globalEc.setValue(CB, res);
            }
            if (InvokeInst* II = dyn_cast<InvokeInst>(CB))
            {
                switchToNewBasicBlock(II->getNormalDest (), callingEc, _globalEc);
            }
            // We returned from the call...
            callingEc.caller = nullptr;
        }
     }
 }
 
 void LlvmIrEmulator::visitReturnInst(llvm::ReturnInst& I)
 {
     LocalExecutionContext& ec = _ecStack.back();
     Type* retTy = Type::getVoidTy(I.getContext());
     GenericValue res;
 
     // Save away the return value... (if we are not 'ret void')
     if (I.getNumOperands())
     {
         retTy = I.getReturnValue()->getType();
         res = _globalEc.getOperandValue(I.getReturnValue(), ec);
     }
 
     popStackAndReturnValueToCaller(retTy, res);
 }
 
 void LlvmIrEmulator::visitUnreachableInst(llvm::UnreachableInst& I)
 {
     throw LlvmIrEmulatorError("Program executed an 'unreachable' instruction!");
 }
 
 void LlvmIrEmulator::visitBranchInst(llvm::BranchInst& I)
 {
     LocalExecutionContext& ec = _ecStack.back();
     BasicBlock* dest;
 
     dest = I.getSuccessor(0);
     if (!I.isUnconditional())
     {
         Value* cond = I.getCondition();
         if (_globalEc.getOperandValue(cond, ec).IntVal == false)
         {
             dest = I.getSuccessor(1);
         }
     }
     switchToNewBasicBlock(dest, ec, _globalEc);
 }
 
 void LlvmIrEmulator::visitSwitchInst(llvm::SwitchInst& I)
 {
     LocalExecutionContext& ec = _ecStack.back();
     Value* cond = I.getCondition();
     Type* elTy = cond->getType();
     GenericValue condVal = _globalEc.getOperandValue(cond, ec);
 
     // Check to see if any of the cases match...
     BasicBlock *dest = nullptr;
     for (auto Case : I.cases())
     {
         GenericValue caseVal = _globalEc.getOperandValue(Case.getCaseValue(), ec);
         if (executeICMP_EQ(condVal, caseVal, elTy).IntVal != 0)
         {
             dest = cast<BasicBlock>(Case.getCaseSuccessor());
             break;
         }
     }
     if (!dest)
     {
         dest = I.getDefaultDest();   // No cases matched: use default
     }
     switchToNewBasicBlock(dest, ec, _globalEc);
 }
 
 void LlvmIrEmulator::visitIndirectBrInst(llvm::IndirectBrInst& I)
 {
     LocalExecutionContext& ec = _ecStack.back();
     void* dest = GVTOP(_globalEc.getOperandValue(I.getAddress(), ec));
     switchToNewBasicBlock(reinterpret_cast<BasicBlock*>(dest), ec, _globalEc);
 }

} // llvmir_emul
} // retdec
