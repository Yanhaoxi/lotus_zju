/**
 * @file src/llvmir-emul/emulator/instruction_visitors_simple.cpp
 * @brief Simple instruction visitor implementations (misc, call, conversion).
 * @copyright (c) 2017 Avast Software, licensed under the MIT license
 */

#include "Analysis/llvmir-emul/llvmir_emul.h"
#include <llvm/IR/Instructions.h>
#include <llvm/CodeGen/IntrinsicLowering.h>

using namespace llvm;

namespace retdec {
namespace llvmir_emul {

//=============================================================================
// Miscellaneous Instruction Visitors
//=============================================================================

void LlvmIrEmulator::visitVAArgInst(llvm::VAArgInst& I)
{
    assert(false && "Handling of VAArgInst is not implemented");
    throw LlvmIrEmulatorError("Handling of VAArgInst is not implemented");
}

/**
 * This is not really getting the value. It just sets ExtractValueInst's result
 * to uninitialized GenericValue.
 */
void LlvmIrEmulator::visitExtractElementInst(llvm::ExtractElementInst& I)
{
    GenericValue dest;
    _globalEc.setValue(&I, dest);
}

void LlvmIrEmulator::visitInsertElementInst(llvm::InsertElementInst& I)
{
    assert(false && "Handling of InsertElementInst is not implemented");
    throw LlvmIrEmulatorError("Handling of InsertElementInst is not implemented");
}

void LlvmIrEmulator::visitShuffleVectorInst(llvm::ShuffleVectorInst& I)
{
    assert(false && "Handling of ShuffleVectorInst is not implemented");
    throw LlvmIrEmulatorError("Handling of ShuffleVectorInst is not implemented");
}

/**
 * This is not really getting the value. It just sets ExtractValueInst's result
 * to uninitialized GenericValue.
 */
void LlvmIrEmulator::visitExtractValueInst(llvm::ExtractValueInst& I)
{
    GenericValue dest;
    _globalEc.setValue(&I, dest);
}

void LlvmIrEmulator::visitInsertValueInst(llvm::InsertValueInst& I)
{
    assert(false && "Handling of InsertValueInst is not implemented");
    throw LlvmIrEmulatorError("Handling of InsertValueInst is not implemented");
}

void LlvmIrEmulator::visitPHINode(llvm::PHINode& PN)
{
    throw LlvmIrEmulatorError("PHI nodes already handled!");
}

//=============================================================================
// Call Instruction Visitors
//=============================================================================

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

//=============================================================================
// Conversion Instruction Visitors
//=============================================================================

void LlvmIrEmulator::visitTruncInst(llvm::TruncInst& I)
{
    LocalExecutionContext& ec = _ecStack.back();
    _globalEc.setValue(&I, executeTruncInst(I.getOperand(0), I.getType(), ec, _globalEc));
}
 
void LlvmIrEmulator::visitSExtInst(llvm::SExtInst& I)
{
    LocalExecutionContext& ec = _ecStack.back();
    _globalEc.setValue(&I, executeSExtInst(I.getOperand(0), I.getType(), ec, _globalEc));
}
 
void LlvmIrEmulator::visitZExtInst(llvm::ZExtInst& I)
{
    LocalExecutionContext& ec = _ecStack.back();
    _globalEc.setValue(&I, executeZExtInst(I.getOperand(0), I.getType(), ec, _globalEc));
}
 
void LlvmIrEmulator::visitFPTruncInst(llvm::FPTruncInst& I)
{
    LocalExecutionContext& ec = _ecStack.back();
    _globalEc.setValue(&I, executeFPTruncInst(I.getOperand(0), I.getType(), ec, _globalEc));
}
 
void LlvmIrEmulator::visitFPExtInst(llvm::FPExtInst& I)
{
    LocalExecutionContext& ec = _ecStack.back();
    _globalEc.setValue(&I, executeFPExtInst(I.getOperand(0), I.getType(), ec, _globalEc));
}
 
void LlvmIrEmulator::visitUIToFPInst(llvm::UIToFPInst& I)
{
    LocalExecutionContext& ec = _ecStack.back();
    _globalEc.setValue(&I, executeUIToFPInst(I.getOperand(0), I.getType(), ec, _globalEc));
}
 
void LlvmIrEmulator::visitSIToFPInst(llvm::SIToFPInst& I)
{
    LocalExecutionContext& ec = _ecStack.back();
    _globalEc.setValue(&I, executeSIToFPInst(I.getOperand(0), I.getType(), ec, _globalEc));
}
 
void LlvmIrEmulator::visitFPToUIInst(llvm::FPToUIInst& I)
{
    LocalExecutionContext& ec = _ecStack.back();
    _globalEc.setValue(&I, executeFPToUIInst(I.getOperand(0), I.getType(), ec, _globalEc));
}
 
void LlvmIrEmulator::visitFPToSIInst(llvm::FPToSIInst& I)
{
    LocalExecutionContext& ec = _ecStack.back();
    _globalEc.setValue(&I, executeFPToSIInst(I.getOperand(0), I.getType(), ec, _globalEc));
}
 
void LlvmIrEmulator::visitPtrToIntInst(llvm::PtrToIntInst& I)
{
    LocalExecutionContext& ec = _ecStack.back();
    _globalEc.setValue(&I, executePtrToIntInst(I.getOperand(0), I.getType(), ec, _globalEc));
}
 
void LlvmIrEmulator::visitIntToPtrInst(llvm::IntToPtrInst& I)
{
    LocalExecutionContext& ec = _ecStack.back();
    _globalEc.setValue(&I, executeIntToPtrInst(I.getOperand(0), I.getType(), ec, _globalEc));
}
 
void LlvmIrEmulator::visitBitCastInst(llvm::BitCastInst& I)
{
    LocalExecutionContext& ec = _ecStack.back();
    _globalEc.setValue(&I, executeBitCastInst(I.getOperand(0), I.getType(), ec, _globalEc));
}

} // llvmir_emul
} // retdec

