/**
 * @file src/llvmir-emul/instruction_visitors_conversion.cpp
 * @brief Conversion instruction visitor implementations.
 * @copyright (c) 2017 Avast Software, licensed under the MIT license
 */

#include "Analysis/llvmir-emul/llvmir_emul.h"
#include <llvm/IR/Instructions.h>

using namespace llvm;

namespace retdec {
namespace llvmir_emul {
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
