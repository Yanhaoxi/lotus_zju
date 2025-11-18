/**
 * @file src/llvmir-emul/instruction_visitors_shift.cpp
 * @brief Shift instruction visitor implementations.
 * @copyright (c) 2017 Avast Software, licensed under the MIT license
 */

#include "Analysis/llvmir-emul/llvmir_emul.h"
#include <llvm/IR/Instructions.h>
#include <llvm/Support/MathExtras.h>

using namespace llvm;

namespace retdec {
namespace llvmir_emul {
 void LlvmIrEmulator::visitShl(llvm::BinaryOperator& I)
 {
     LocalExecutionContext& ec = _ecStack.back();
     GenericValue op0 = _globalEc.getOperandValue(I.getOperand(0), ec);
     GenericValue op1 = _globalEc.getOperandValue(I.getOperand(1), ec);
     GenericValue Dest;
     Type* ty = I.getType();
 
     if (ty->isVectorTy())
     {
         uint32_t src1Size = uint32_t(op0.AggregateVal.size());
         assert(src1Size == op1.AggregateVal.size());
         for (unsigned i = 0; i < src1Size; i++)
         {
             GenericValue Result;
             uint64_t shiftAmount = op1.AggregateVal[i].IntVal.getZExtValue();
             llvm::APInt valueToShift = op0.AggregateVal[i].IntVal;
             Result.IntVal = valueToShift.shl(getShiftAmount(shiftAmount, valueToShift));
             Dest.AggregateVal.push_back(Result);
         }
     }
     else
     {
         // scalar
         uint64_t shiftAmount = op1.IntVal.getZExtValue();
         llvm::APInt valueToShift = op0.IntVal;
         Dest.IntVal = valueToShift.shl(getShiftAmount(shiftAmount, valueToShift));
     }
 
     _globalEc.setValue(&I, Dest);
 }
 
 void LlvmIrEmulator::visitLShr(llvm::BinaryOperator& I)
 {
     LocalExecutionContext& ec = _ecStack.back();
     GenericValue op0 = _globalEc.getOperandValue(I.getOperand(0), ec);
     GenericValue op1 = _globalEc.getOperandValue(I.getOperand(1), ec);
     GenericValue Dest;
     Type* ty = I.getType();
 
     if (ty->isVectorTy())
     {
         uint32_t src1Size = uint32_t(op0.AggregateVal.size());
         assert(src1Size == op1.AggregateVal.size());
         for (unsigned i = 0; i < src1Size; i++)
         {
             GenericValue Result;
             uint64_t shiftAmount = op1.AggregateVal[i].IntVal.getZExtValue();
             llvm::APInt valueToShift = op0.AggregateVal[i].IntVal;
             Result.IntVal = valueToShift.lshr(getShiftAmount(shiftAmount, valueToShift));
             Dest.AggregateVal.push_back(Result);
         }
     }
     else
     {
         // scalar
         uint64_t shiftAmount = op1.IntVal.getZExtValue();
         llvm::APInt valueToShift = op0.IntVal;
         Dest.IntVal = valueToShift.lshr(getShiftAmount(shiftAmount, valueToShift));
     }
 
     _globalEc.setValue(&I, Dest);
 }
 
 void LlvmIrEmulator::visitAShr(llvm::BinaryOperator& I)
 {
     LocalExecutionContext& ec = _ecStack.back();
     GenericValue op0 = _globalEc.getOperandValue(I.getOperand(0), ec);
     GenericValue op1 = _globalEc.getOperandValue(I.getOperand(1), ec);
     GenericValue Dest;
     Type* ty = I.getType();
 
     if (ty->isVectorTy())
     {
         size_t src1Size = op0.AggregateVal.size();
         assert(src1Size == op1.AggregateVal.size());
         for (unsigned i = 0; i < src1Size; i++)
         {
             GenericValue Result;
             uint64_t shiftAmount = op1.AggregateVal[i].IntVal.getZExtValue();
             llvm::APInt valueToShift = op0.AggregateVal[i].IntVal;
             Result.IntVal = valueToShift.ashr(getShiftAmount(shiftAmount, valueToShift));
             Dest.AggregateVal.push_back(Result);
         }
     }
     else
     {
         // scalar
         uint64_t shiftAmount = op1.IntVal.getZExtValue();
         llvm::APInt valueToShift = op0.IntVal;
         Dest.IntVal = valueToShift.ashr(getShiftAmount(shiftAmount, valueToShift));
     }
 
     _globalEc.setValue(&I, Dest);
 }

} // llvmir_emul
} // retdec
