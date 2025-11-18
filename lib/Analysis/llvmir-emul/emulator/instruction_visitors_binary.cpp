/**
 * @file src/llvmir-emul/instruction_visitors_binary.cpp
 * @brief Binary and comparison instruction visitor implementations.
 * @copyright (c) 2017 Avast Software, licensed under the MIT license
 */

#include "Analysis/llvmir-emul/llvmir_emul.h"
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/raw_ostream.h>
#include <cmath>

using namespace llvm;

namespace retdec {
namespace llvmir_emul {
 void LlvmIrEmulator::visitBinaryOperator(llvm::BinaryOperator& I)
 {
     LocalExecutionContext& ec = _ecStack.back();
     Type* ty = I.getOperand(0)->getType();
     GenericValue op0 = _globalEc.getOperandValue(I.getOperand(0), ec);
     GenericValue op1 = _globalEc.getOperandValue(I.getOperand(1), ec);
     GenericValue res;
 
     // First process vector operation
     if (ty->isVectorTy())
     {
         assert(op0.AggregateVal.size() == op1.AggregateVal.size());
         res.AggregateVal.resize(op0.AggregateVal.size());
 
         // Macros to execute binary operation 'OP' over integer vectors
 #define INTEGER_VECTOR_OPERATION(OP)                                   \
     for (unsigned i = 0; i < res.AggregateVal.size(); ++i)             \
         res.AggregateVal[i].IntVal =                                   \
         op0.AggregateVal[i].IntVal OP op1.AggregateVal[i].IntVal;
 
         // Additional macros to execute binary operations udiv/sdiv/urem/srem since
         // they have different notation.
 #define INTEGER_VECTOR_FUNCTION(OP)                                    \
     for (unsigned i = 0; i < res.AggregateVal.size(); ++i)             \
         res.AggregateVal[i].IntVal =                                   \
         op0.AggregateVal[i].IntVal.OP(op1.AggregateVal[i].IntVal);
 
         // Macros to execute binary operation 'OP' over floating point type TY
         // (float or double) vectors
 #define FLOAT_VECTOR_FUNCTION(OP, TY)                                 \
     for (unsigned i = 0; i < res.AggregateVal.size(); ++i)            \
         res.AggregateVal[i].TY =                                      \
         op0.AggregateVal[i].TY OP op1.AggregateVal[i].TY;
 
         // Macros to choose appropriate TY: float or double and run operation
         // execution
 #define FLOAT_VECTOR_OP(OP) {                                               \
     if (cast<VectorType>(ty)->getElementType()->isFloatTy())                \
         FLOAT_VECTOR_FUNCTION(OP, FloatVal)                                 \
     else                                                                    \
     {                                                                       \
         if (cast<VectorType>(ty)->getElementType()->isDoubleTy())           \
             FLOAT_VECTOR_FUNCTION(OP, DoubleVal)                            \
         else                                                                \
         {                                                                   \
             dbgs() << "Unhandled type for OP instruction: " << *ty << "\n"; \
             llvm_unreachable(0);                                            \
         }                                                                   \
     }                                                                       \
 }
 
         switch(I.getOpcode())
         {
             default:
                 dbgs() << "Don't know how to handle this binary operator!\n-->" << I;
                 llvm_unreachable(nullptr);
                 break;
             case Instruction::Add:   INTEGER_VECTOR_OPERATION(+) break;
             case Instruction::Sub:   INTEGER_VECTOR_OPERATION(-) break;
             case Instruction::Mul:   INTEGER_VECTOR_OPERATION(*) break;
             case Instruction::UDiv:  INTEGER_VECTOR_FUNCTION(udiv) break;
             case Instruction::SDiv:  INTEGER_VECTOR_FUNCTION(sdiv) break;
             case Instruction::URem:  INTEGER_VECTOR_FUNCTION(urem) break;
             case Instruction::SRem:  INTEGER_VECTOR_FUNCTION(srem) break;
             case Instruction::And:   INTEGER_VECTOR_OPERATION(&) break;
             case Instruction::Or:    INTEGER_VECTOR_OPERATION(|) break;
             case Instruction::Xor:   INTEGER_VECTOR_OPERATION(^) break;
             case Instruction::FAdd:  FLOAT_VECTOR_OP(+) break;
             case Instruction::FSub:  FLOAT_VECTOR_OP(-) break;
             case Instruction::FMul:  FLOAT_VECTOR_OP(*) break;
             case Instruction::FDiv:  FLOAT_VECTOR_OP(/) break;
             case Instruction::FRem:
             {
                 if (cast<VectorType>(ty)->getElementType()->isFloatTy())
                 {
                     for (unsigned i = 0; i < res.AggregateVal.size(); ++i)
                         res.AggregateVal[i].FloatVal =
                         fmod(op0.AggregateVal[i].FloatVal, op1.AggregateVal[i].FloatVal);
                 }
                 else
                 {
                     if (cast<VectorType>(ty)->getElementType()->isDoubleTy())
                     {
                         for (unsigned i = 0; i < res.AggregateVal.size(); ++i)
                             res.AggregateVal[i].DoubleVal =
                             fmod(op0.AggregateVal[i].DoubleVal, op1.AggregateVal[i].DoubleVal);
                     }
                     else
                     {
                         dbgs() << "Unhandled type for Rem instruction: " << *ty << "\n";
                         llvm_unreachable(nullptr);
                     }
                 }
                 break;
             }
         }
     }
     else
     {
         // Values may not have equal bit sizes, if one was created from fp128
         // or something like that - it would get transformed to double, that
         // to i64, but the original integer operation would have the original
         // large type like i128.
         // Change bitsizes to be the same.
         //
         if (op0.IntVal.getBitWidth() < op1.IntVal.getBitWidth())
         {
             op0.IntVal = APInt(op1.IntVal.getBitWidth(), op0.IntVal.getZExtValue());
         }
         else if (op0.IntVal.getBitWidth() > op1.IntVal.getBitWidth())
         {
             op1.IntVal = APInt(op0.IntVal.getBitWidth(), op1.IntVal.getZExtValue());
         }
 
         switch (I.getOpcode())
         {
             default:
                 dbgs() << "Don't know how to handle this binary operator!\n-->" << I;
                 llvm_unreachable(nullptr);
                 break;
             case Instruction::Add:
                 res.IntVal = op0.IntVal + op1.IntVal;
                 break;
             case Instruction::Sub:   res.IntVal = op0.IntVal - op1.IntVal; break;
             case Instruction::Mul:   res.IntVal = op0.IntVal * op1.IntVal; break;
             case Instruction::FAdd:  executeFAddInst(res, op0, op1, ty); break;
             case Instruction::FSub:  executeFSubInst(res, op0, op1, ty); break;
             case Instruction::FMul:  executeFMulInst(res, op0, op1, ty); break;
             case Instruction::FDiv:  executeFDivInst(res, op0, op1, ty); break;
             case Instruction::FRem:  executeFRemInst(res, op0, op1, ty); break;
             case Instruction::UDiv:  res.IntVal = op0.IntVal.udiv(op1.IntVal); break;
             case Instruction::SDiv:  res.IntVal = op0.IntVal.sdiv(op1.IntVal); break;
             case Instruction::URem:  res.IntVal = op0.IntVal.urem(op1.IntVal); break;
             case Instruction::SRem:  res.IntVal = op0.IntVal.srem(op1.IntVal); break;
             case Instruction::And:   res.IntVal = op0.IntVal & op1.IntVal; break;
             case Instruction::Or:    res.IntVal = op0.IntVal | op1.IntVal; break;
             case Instruction::Xor:   res.IntVal = op0.IntVal ^ op1.IntVal; break;
         }
     }
 
     _globalEc.setValue(&I, res);
 }
 
 void LlvmIrEmulator::visitICmpInst(llvm::ICmpInst& I)
 {
     LocalExecutionContext& ec = _ecStack.back();
     Type* ty = I.getOperand(0)->getType();
     GenericValue op0 = _globalEc.getOperandValue(I.getOperand(0), ec);
     GenericValue op1 = _globalEc.getOperandValue(I.getOperand(1), ec);
     GenericValue res;
 
     switch (I.getPredicate())
     {
         case ICmpInst::ICMP_EQ:  res = executeICMP_EQ(op0,  op1, ty); break;
         case ICmpInst::ICMP_NE:  res = executeICMP_NE(op0,  op1, ty); break;
         case ICmpInst::ICMP_ULT: res = executeICMP_ULT(op0, op1, ty); break;
         case ICmpInst::ICMP_SLT: res = executeICMP_SLT(op0, op1, ty); break;
         case ICmpInst::ICMP_UGT: res = executeICMP_UGT(op0, op1, ty); break;
         case ICmpInst::ICMP_SGT: res = executeICMP_SGT(op0, op1, ty); break;
         case ICmpInst::ICMP_ULE: res = executeICMP_ULE(op0, op1, ty); break;
         case ICmpInst::ICMP_SLE: res = executeICMP_SLE(op0, op1, ty); break;
         case ICmpInst::ICMP_UGE: res = executeICMP_UGE(op0, op1, ty); break;
         case ICmpInst::ICMP_SGE: res = executeICMP_SGE(op0, op1, ty); break;
         default:
             dbgs() << "Don't know how to handle this ICmp predicate!\n-->" << I;
             llvm_unreachable(nullptr);
     }
 
     _globalEc.setValue(&I, res);
 }
 
 void LlvmIrEmulator::visitFCmpInst(llvm::FCmpInst& I)
 {
     LocalExecutionContext& ec = _ecStack.back();
     Type* ty = I.getOperand(0)->getType();
     GenericValue op0 = _globalEc.getOperandValue(I.getOperand(0), ec);
     GenericValue op1 = _globalEc.getOperandValue(I.getOperand(1), ec);
     GenericValue res;
 
     switch (I.getPredicate())
     {
         default:
             dbgs() << "Don't know how to handle this FCmp predicate!\n-->" << I;
             llvm_unreachable(nullptr);
             break;
         case FCmpInst::FCMP_FALSE: res = executeFCMP_BOOL(op0, op1, ty, false); break;
         case FCmpInst::FCMP_TRUE:  res = executeFCMP_BOOL(op0, op1, ty, true); break;
         case FCmpInst::FCMP_ORD:   res = executeFCMP_ORD(op0, op1, ty); break;
         case FCmpInst::FCMP_UNO:   res = executeFCMP_UNO(op0, op1, ty); break;
         case FCmpInst::FCMP_UEQ:   res = executeFCMP_UEQ(op0, op1, ty); break;
         case FCmpInst::FCMP_OEQ:   res = executeFCMP_OEQ(op0, op1, ty); break;
         case FCmpInst::FCMP_UNE:   res = executeFCMP_UNE(op0, op1, ty); break;
         case FCmpInst::FCMP_ONE:   res = executeFCMP_ONE(op0, op1, ty); break;
         case FCmpInst::FCMP_ULT:   res = executeFCMP_ULT(op0, op1, ty); break;
         case FCmpInst::FCMP_OLT:   res = executeFCMP_OLT(op0, op1, ty); break;
         case FCmpInst::FCMP_UGT:   res = executeFCMP_UGT(op0, op1, ty); break;
         case FCmpInst::FCMP_OGT:   res = executeFCMP_OGT(op0, op1, ty); break;
         case FCmpInst::FCMP_ULE:   res = executeFCMP_ULE(op0, op1, ty); break;
         case FCmpInst::FCMP_OLE:   res = executeFCMP_OLE(op0, op1, ty); break;
         case FCmpInst::FCMP_UGE:   res = executeFCMP_UGE(op0, op1, ty); break;
         case FCmpInst::FCMP_OGE:   res = executeFCMP_OGE(op0, op1, ty); break;
     }
 
     _globalEc.setValue(&I, res);
 }
 
 //
 //=============================================================================
 // Ternary Instruction Implementations
 //=============================================================================
 //
 
 void LlvmIrEmulator::visitSelectInst(llvm::SelectInst& I)
 {
     LocalExecutionContext& ec = _ecStack.back();
     Type* ty = I.getOperand(0)->getType();
     GenericValue op0 = _globalEc.getOperandValue(I.getOperand(0), ec);
     GenericValue op1 = _globalEc.getOperandValue(I.getOperand(1), ec);
     GenericValue op2 = _globalEc.getOperandValue(I.getOperand(2), ec);
     GenericValue res = executeSelectInst(op0, op1, op2, ty);
     _globalEc.setValue(&I, res);
 }

} // llvmir_emul
} // retdec
