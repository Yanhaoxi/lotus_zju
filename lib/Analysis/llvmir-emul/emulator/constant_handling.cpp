/**
 * @file src/llvmir-emul/constant_handling.cpp
 * @brief Constant value processing implementations.

 */

#include <llvm/IR/Constants.h>
#include <llvm/IR/GetElementPtrTypeIterator.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/ErrorHandling.h>

#include "Analysis/llvmir-emul/llvmir_emul.h"

using namespace llvm;

namespace retdec {
namespace llvmir_emul {

// Forward declarations
GenericValue executeTruncInst(Value *SrcVal, Type *DstTy, LocalExecutionContext &SF, GlobalExecutionContext& GC);
GenericValue executeZExtInst(Value *SrcVal, Type *DstTy, LocalExecutionContext &SF, GlobalExecutionContext& GC);
GenericValue executeSExtInst(Value *SrcVal, Type *DstTy, LocalExecutionContext &SF, GlobalExecutionContext& GC);
GenericValue executeFPTruncInst(Value *SrcVal, Type *DstTy, LocalExecutionContext &SF, GlobalExecutionContext& GC);
GenericValue executeFPExtInst(Value *SrcVal, Type *DstTy, LocalExecutionContext &SF, GlobalExecutionContext& GC);
GenericValue executeUIToFPInst(Value *SrcVal, Type *DstTy, LocalExecutionContext &SF, GlobalExecutionContext& GC);
GenericValue executeSIToFPInst(Value *SrcVal, Type *DstTy, LocalExecutionContext &SF, GlobalExecutionContext& GC);
GenericValue executeFPToUIInst(Value *SrcVal, Type *DstTy, LocalExecutionContext &SF, GlobalExecutionContext& GC);
GenericValue executeFPToSIInst(Value *SrcVal, Type *DstTy, LocalExecutionContext &SF, GlobalExecutionContext& GC);
GenericValue executePtrToIntInst(Value *SrcVal, Type *DstTy, LocalExecutionContext &SF, GlobalExecutionContext& GC);
GenericValue executeIntToPtrInst(Value *SrcVal, Type *DstTy, LocalExecutionContext &SF, GlobalExecutionContext& GC);
GenericValue executeBitCastInst(Value *SrcVal, Type *DstTy, LocalExecutionContext &SF, GlobalExecutionContext& GC);
GenericValue executeGEPOperation(Value *Ptr, gep_type_iterator I, gep_type_iterator E, LocalExecutionContext& SF, GlobalExecutionContext& GC);
GenericValue executeCmpInst(unsigned predicate, GenericValue Src1, GenericValue Src2, Type *Ty);
GenericValue executeSelectInst(GenericValue Src1, GenericValue Src2, GenericValue Src3, Type *Ty);
void executeFAddInst(GenericValue &Dest, GenericValue Src1, GenericValue Src2, Type *Ty);
void executeFSubInst(GenericValue &Dest, GenericValue Src1, GenericValue Src2, Type *Ty);
void executeFMulInst(GenericValue &Dest, GenericValue Src1, GenericValue Src2, Type *Ty);
void executeFDivInst(GenericValue &Dest, GenericValue Src1, GenericValue Src2, Type *Ty);
void executeFRemInst(GenericValue &Dest, GenericValue Src1, GenericValue Src2, Type *Ty);

llvm::GenericValue getConstantExprValue(
         llvm::ConstantExpr* CE,
         LocalExecutionContext& SF,
         GlobalExecutionContext& GC)
 {
     switch (CE->getOpcode())
     {
         case Instruction::Trunc:
             return executeTruncInst(CE->getOperand(0), CE->getType(), SF, GC);
         case Instruction::ZExt:
             return executeZExtInst(CE->getOperand(0), CE->getType(), SF, GC);
         case Instruction::SExt:
             return executeSExtInst(CE->getOperand(0), CE->getType(), SF, GC);
         case Instruction::FPTrunc:
             return executeFPTruncInst(CE->getOperand(0), CE->getType(), SF, GC);
         case Instruction::FPExt:
             return executeFPExtInst(CE->getOperand(0), CE->getType(), SF, GC);
         case Instruction::UIToFP:
             return executeUIToFPInst(CE->getOperand(0), CE->getType(), SF, GC);
         case Instruction::SIToFP:
             return executeSIToFPInst(CE->getOperand(0), CE->getType(), SF, GC);
         case Instruction::FPToUI:
             return executeFPToUIInst(CE->getOperand(0), CE->getType(), SF, GC);
         case Instruction::FPToSI:
             return executeFPToSIInst(CE->getOperand(0), CE->getType(), SF, GC);
         case Instruction::PtrToInt:
             return executePtrToIntInst(CE->getOperand(0), CE->getType(), SF, GC);
         case Instruction::IntToPtr:
             return executeIntToPtrInst(CE->getOperand(0), CE->getType(), SF, GC);
         case Instruction::BitCast:
             return executeBitCastInst(CE->getOperand(0), CE->getType(), SF, GC);
         case Instruction::GetElementPtr:
             return executeGEPOperation(CE->getOperand(0), gep_type_begin(CE),
                     gep_type_end(CE), SF, GC);
         case Instruction::FCmp:
         case Instruction::ICmp:
             return executeCmpInst(
                     CE->getPredicate(),
                     GC.getOperandValue(CE->getOperand(0), SF),
                     GC.getOperandValue(CE->getOperand(1), SF),
                     CE->getOperand(0)->getType());
         case Instruction::Select:
             return executeSelectInst(
                     GC.getOperandValue(CE->getOperand(0), SF),
                     GC.getOperandValue(CE->getOperand(1), SF),
                     GC.getOperandValue(CE->getOperand(2), SF),
                     CE->getOperand(0)->getType());
         default :
             break;
     }
 
     // The cases below here require a GenericValue parameter for the result
     // so we initialize one, compute it and then return it.
     GenericValue Op0 = GC.getOperandValue(CE->getOperand(0), SF);
     GenericValue Op1 = GC.getOperandValue(CE->getOperand(1), SF);
     GenericValue Dest;
     Type * Ty = CE->getOperand(0)->getType();
     switch (CE->getOpcode())
     {
         case Instruction::Add:  Dest.IntVal = Op0.IntVal + Op1.IntVal; break;
         case Instruction::Sub:  Dest.IntVal = Op0.IntVal - Op1.IntVal; break;
         case Instruction::Mul:  Dest.IntVal = Op0.IntVal * Op1.IntVal; break;
         case Instruction::FAdd: executeFAddInst(Dest, Op0, Op1, Ty); break;
         case Instruction::FSub: executeFSubInst(Dest, Op0, Op1, Ty); break;
         case Instruction::FMul: executeFMulInst(Dest, Op0, Op1, Ty); break;
         case Instruction::FDiv: executeFDivInst(Dest, Op0, Op1, Ty); break;
         case Instruction::FRem: executeFRemInst(Dest, Op0, Op1, Ty); break;
         case Instruction::SDiv: Dest.IntVal = Op0.IntVal.sdiv(Op1.IntVal); break;
         case Instruction::UDiv: Dest.IntVal = Op0.IntVal.udiv(Op1.IntVal); break;
         case Instruction::URem: Dest.IntVal = Op0.IntVal.urem(Op1.IntVal); break;
         case Instruction::SRem: Dest.IntVal = Op0.IntVal.srem(Op1.IntVal); break;
         case Instruction::And:  Dest.IntVal = Op0.IntVal & Op1.IntVal; break;
         case Instruction::Or:   Dest.IntVal = Op0.IntVal | Op1.IntVal; break;
         case Instruction::Xor:  Dest.IntVal = Op0.IntVal ^ Op1.IntVal; break;
         case Instruction::Shl:
             Dest.IntVal = Op0.IntVal.shl(Op1.IntVal.getZExtValue());
             break;
         case Instruction::LShr:
             Dest.IntVal = Op0.IntVal.lshr(Op1.IntVal.getZExtValue());
             break;
         case Instruction::AShr:
             Dest.IntVal = Op0.IntVal.ashr(Op1.IntVal.getZExtValue());
             break;
         default:
             dbgs() << "Unhandled ConstantExpr: " << *CE << "\n";
             llvm_unreachable("Unhandled ConstantExpr");
     }
     return Dest;
 }
 
 /**
  * Converts a Constant* into a GenericValue, including handling of
  * ConstantExpr values.
  * Taken from ExecutionEngine/ExecutionEngine.cpp
  */
 llvm::GenericValue getConstantValue(const llvm::Constant* C, llvm::Module* m)
 {
     auto& DL = m->getDataLayout();
 
     // If its undefined, return the garbage.
     if (isa<UndefValue>(C))
     {
         GenericValue Result;
         switch (C->getType()->getTypeID())
         {
            case Type::IntegerTyID:
             case Type::X86_FP80TyID:
             case Type::FP128TyID:
             case Type::PPC_FP128TyID:
                 // Although the value is undefined, we still have to construct an APInt
                 // with the correct bit width.
                 Result.IntVal = APInt(C->getType()->getPrimitiveSizeInBits(), 0);
                 break;
             case Type::StructTyID:
             {
                 // if the whole struct is 'undef' just reserve memory for the value.
                 if(StructType *STy = dyn_cast<StructType>(C->getType()))
                 {
                     unsigned int elemNum = STy->getNumElements();
                     Result.AggregateVal.resize(elemNum);
                     for (unsigned int i = 0; i < elemNum; ++i)
                     {
                         Type *ElemTy = STy->getElementType(i);
                         if (ElemTy->isIntegerTy())
                         {
                             Result.AggregateVal[i].IntVal =
                                     APInt(ElemTy->getPrimitiveSizeInBits(), 0);
                         }
                         else if (ElemTy->isAggregateType())
                         {
                             const Constant *ElemUndef = UndefValue::get(ElemTy);
                             Result.AggregateVal[i] = getConstantValue(ElemUndef, m);
                         }
                     }
                 }
                 break;
            }
            default:
                if (C->getType()->isVectorTy()) {
                // if the whole vector is 'undef' just reserve memory for the value.
                auto* VTy = dyn_cast<VectorType>(C->getType());
                Type *ElemTy = VTy->getElementType();
                unsigned int elemNum = VTy->getElementCount().getFixedValue();
                 Result.AggregateVal.resize(elemNum);
                if (ElemTy->isIntegerTy())
                    for (unsigned int i = 0; i < elemNum; ++i)
                        Result.AggregateVal[i].IntVal =
                                APInt(ElemTy->getPrimitiveSizeInBits(), 0);
                break;
                }
         }
         return Result;
     }
 
     // Otherwise, if the value is a ConstantExpr...
     if (const ConstantExpr *CE = dyn_cast<ConstantExpr>(C))
     {
         Constant *Op0 = CE->getOperand(0);
         switch (CE->getOpcode())
         {
             case Instruction::GetElementPtr:
             {
                 // Compute the index
                 GenericValue Result = getConstantValue(Op0, m);
                 APInt Offset(DL.getPointerSizeInBits(), 0);
                 cast<GEPOperator>(CE)->accumulateConstantOffset(DL, Offset);
 
                 char* tmp = static_cast<char*>(Result.PointerVal);
                 Result = PTOGV(tmp + Offset.getSExtValue());
                 return Result;
             }
             case Instruction::Trunc:
             {
                 GenericValue GV = getConstantValue(Op0, m);
                 uint32_t BitWidth = cast<IntegerType>(CE->getType())->getBitWidth();
                 GV.IntVal = GV.IntVal.trunc(BitWidth);
                 return GV;
             }
             case Instruction::ZExt:
             {
                 GenericValue GV = getConstantValue(Op0, m);
                 uint32_t BitWidth = cast<IntegerType>(CE->getType())->getBitWidth();
                 GV.IntVal = GV.IntVal.zext(BitWidth);
                 return GV;
             }
             case Instruction::SExt:
             {
                 GenericValue GV = getConstantValue(Op0, m);
                 uint32_t BitWidth = cast<IntegerType>(CE->getType())->getBitWidth();
                 GV.IntVal = GV.IntVal.sext(BitWidth);
                 return GV;
             }
             case Instruction::FPTrunc:
             {
                 GenericValue GV = getConstantValue(Op0, m);
                 GV.FloatVal = float(GV.DoubleVal);
                 return GV;
             }
             case Instruction::FPExt:
             {
                 GenericValue GV = getConstantValue(Op0, m);
                 GV.DoubleVal = double(GV.FloatVal);
                 return GV;
             }
             case Instruction::UIToFP:
             {
                 GenericValue GV = getConstantValue(Op0, m);
                 if (CE->getType()->isFloatTy())
                     GV.FloatVal = float(GV.IntVal.roundToDouble());
                 else if (CE->getType()->isDoubleTy() || CE->getType()->isX86_FP80Ty())
                     GV.DoubleVal = GV.IntVal.roundToDouble();
                 else if (CE->getType()->isX86_FP80Ty())
                 {
                     APFloat apf = APFloat::getZero(APFloat::x87DoubleExtended());
                     (void)apf.convertFromAPInt(GV.IntVal,
                             false,
                             APFloat::rmNearestTiesToEven);
                     GV.IntVal = apf.bitcastToAPInt();
                 }
                 return GV;
             }
             case Instruction::SIToFP:
             {
                 GenericValue GV = getConstantValue(Op0, m);
                 if (CE->getType()->isFloatTy())
                     GV.FloatVal = float(GV.IntVal.signedRoundToDouble());
                 else if (CE->getType()->isDoubleTy() || CE->getType()->isX86_FP80Ty())
                     GV.DoubleVal = GV.IntVal.signedRoundToDouble();
                 else if (CE->getType()->isX86_FP80Ty())
                 {
                     APFloat apf = APFloat::getZero(APFloat::x87DoubleExtended());
                     (void)apf.convertFromAPInt(GV.IntVal,
                             true,
                             APFloat::rmNearestTiesToEven);
                     GV.IntVal = apf.bitcastToAPInt();
                 }
                 return GV;
             }
             case Instruction::FPToUI: // double->APInt conversion handles sign
             case Instruction::FPToSI:
             {
                 GenericValue GV = getConstantValue(Op0, m);
                 uint32_t BitWidth = cast<IntegerType>(CE->getType())->getBitWidth();
                 if (Op0->getType()->isFloatTy())
                     GV.IntVal = APIntOps::RoundFloatToAPInt(GV.FloatVal, BitWidth);
                 else if (Op0->getType()->isDoubleTy() || CE->getType()->isX86_FP80Ty())
                     GV.IntVal = APIntOps::RoundDoubleToAPInt(GV.DoubleVal, BitWidth);
                 else if (Op0->getType()->isX86_FP80Ty())
                 {
                     APFloat apf = APFloat(APFloat::x87DoubleExtended(), GV.IntVal);
                     uint64_t v;
                     bool ignored;
                     (void)apf.convertToInteger(
                             makeMutableArrayRef(v),
                             BitWidth,
                             CE->getOpcode()==Instruction::FPToSI,
                             APFloat::rmTowardZero,
                             &ignored);
                     GV.IntVal = v; // endian?
                 }
                 return GV;
             }
             case Instruction::PtrToInt:
             {
                 GenericValue GV = getConstantValue(Op0, m);
                 uint32_t PtrWidth = DL.getTypeSizeInBits(Op0->getType());
                 assert(PtrWidth <= 64 && "Bad pointer width");
                 GV.IntVal = APInt(PtrWidth, uintptr_t(GV.PointerVal));
                 uint32_t IntWidth = DL.getTypeSizeInBits(CE->getType());
                 GV.IntVal = GV.IntVal.zextOrTrunc(IntWidth);
                 return GV;
             }
             case Instruction::IntToPtr:
             {
                 GenericValue GV = getConstantValue(Op0, m);
                 uint32_t PtrWidth = DL.getTypeSizeInBits(CE->getType());
                 GV.IntVal = GV.IntVal.zextOrTrunc(PtrWidth);
                 assert(GV.IntVal.getBitWidth() <= 64 && "Bad pointer width");
                 GV.PointerVal = PointerTy(uintptr_t(GV.IntVal.getZExtValue()));
                 return GV;
             }
             case Instruction::BitCast:
             {
                 GenericValue GV = getConstantValue(Op0, m);
                 Type* DestTy = CE->getType();
                 switch (Op0->getType()->getTypeID())
                 {
                     default:
                         llvm_unreachable("Invalid bitcast operand");
                     case Type::IntegerTyID:
                         assert(DestTy->isFloatingPointTy() && "invalid bitcast");
                         if (DestTy->isFloatTy())
                             GV.FloatVal = GV.IntVal.bitsToFloat();
                         else if (DestTy->isDoubleTy())
                             GV.DoubleVal = GV.IntVal.bitsToDouble();
                         break;
                     case Type::FloatTyID:
                         assert(DestTy->isIntegerTy(32) && "Invalid bitcast");
                         GV.IntVal = APInt::floatToBits(GV.FloatVal);
                         break;
                     case Type::DoubleTyID:
                         assert(DestTy->isIntegerTy(64) && "Invalid bitcast");
                         GV.IntVal = APInt::doubleToBits(GV.DoubleVal);
                         break;
                     case Type::PointerTyID:
                         assert(DestTy->isPointerTy() && "Invalid bitcast");
                         break; // getConstantValue(Op0)  above already converted it
                 }
                 return GV;
             }
             case Instruction::Add:
             case Instruction::FAdd:
             case Instruction::Sub:
             case Instruction::FSub:
             case Instruction::Mul:
             case Instruction::FMul:
             case Instruction::UDiv:
             case Instruction::SDiv:
             case Instruction::URem:
             case Instruction::SRem:
             case Instruction::And:
             case Instruction::Or:
             case Instruction::Xor:
             {
                 GenericValue LHS = getConstantValue(Op0, m);
                 GenericValue RHS = getConstantValue(CE->getOperand(1), m);
                 GenericValue GV;
                 switch (CE->getOperand(0)->getType()->getTypeID())
                 {
                     default:
                         llvm_unreachable("Bad add type!");
                     case Type::IntegerTyID:
                         switch (CE->getOpcode())
                         {
                             default: llvm_unreachable("Invalid integer opcode");
                             case Instruction::Add: GV.IntVal = LHS.IntVal + RHS.IntVal; break;
                             case Instruction::Sub: GV.IntVal = LHS.IntVal - RHS.IntVal; break;
                             case Instruction::Mul: GV.IntVal = LHS.IntVal * RHS.IntVal; break;
                             case Instruction::UDiv:GV.IntVal = LHS.IntVal.udiv(RHS.IntVal); break;
                             case Instruction::SDiv:GV.IntVal = LHS.IntVal.sdiv(RHS.IntVal); break;
                             case Instruction::URem:GV.IntVal = LHS.IntVal.urem(RHS.IntVal); break;
                             case Instruction::SRem:GV.IntVal = LHS.IntVal.srem(RHS.IntVal); break;
                             case Instruction::And: GV.IntVal = LHS.IntVal & RHS.IntVal; break;
                             case Instruction::Or:  GV.IntVal = LHS.IntVal | RHS.IntVal; break;
                             case Instruction::Xor: GV.IntVal = LHS.IntVal ^ RHS.IntVal; break;
                         }
                         break;
                     case Type::FloatTyID:
                         switch (CE->getOpcode())
                         {
                             default: llvm_unreachable("Invalid float opcode");
                             case Instruction::FAdd:
                                 GV.FloatVal = LHS.FloatVal + RHS.FloatVal; break;
                             case Instruction::FSub:
                                 GV.FloatVal = LHS.FloatVal - RHS.FloatVal; break;
                             case Instruction::FMul:
                                 GV.FloatVal = LHS.FloatVal * RHS.FloatVal; break;
                             case Instruction::FDiv:
                                 GV.FloatVal = LHS.FloatVal / RHS.FloatVal; break;
                             case Instruction::FRem:
                                 GV.FloatVal = std::fmod(LHS.FloatVal,RHS.FloatVal); break;
                         }
                         break;
                     case Type::DoubleTyID:
                     case Type::X86_FP80TyID:
                         switch (CE->getOpcode())
                         {
                             default: llvm_unreachable("Invalid double opcode");
                             case Instruction::FAdd:
                                 GV.DoubleVal = LHS.DoubleVal + RHS.DoubleVal; break;
                             case Instruction::FSub:
                                 GV.DoubleVal = LHS.DoubleVal - RHS.DoubleVal; break;
                             case Instruction::FMul:
                                 GV.DoubleVal = LHS.DoubleVal * RHS.DoubleVal; break;
                             case Instruction::FDiv:
                                 GV.DoubleVal = LHS.DoubleVal / RHS.DoubleVal; break;
                             case Instruction::FRem:
                                 GV.DoubleVal = std::fmod(LHS.DoubleVal,RHS.DoubleVal); break;
                         }
                         break;
 //					case Type::X86_FP80TyID:
                     case Type::PPC_FP128TyID:
                     case Type::FP128TyID:
                     {
                         const fltSemantics &Sem = CE->getOperand(0)->getType()->getFltSemantics();
                         APFloat apfLHS = APFloat(Sem, LHS.IntVal);
                         switch (CE->getOpcode())
                         {
                             default: llvm_unreachable("Invalid long double opcode");
                             case Instruction::FAdd:
                                 apfLHS.add(APFloat(Sem, RHS.IntVal), APFloat::rmNearestTiesToEven);
                                 GV.IntVal = apfLHS.bitcastToAPInt();
                                 break;
                             case Instruction::FSub:
                                 apfLHS.subtract(APFloat(Sem, RHS.IntVal),
                                         APFloat::rmNearestTiesToEven);
                                 GV.IntVal = apfLHS.bitcastToAPInt();
                                 break;
                             case Instruction::FMul:
                                 apfLHS.multiply(APFloat(Sem, RHS.IntVal),
                                         APFloat::rmNearestTiesToEven);
                                 GV.IntVal = apfLHS.bitcastToAPInt();
                                 break;
                             case Instruction::FDiv:
                                 apfLHS.divide(APFloat(Sem, RHS.IntVal),
                                         APFloat::rmNearestTiesToEven);
                                 GV.IntVal = apfLHS.bitcastToAPInt();
                                 break;
                             case Instruction::FRem:
                                 apfLHS.mod(APFloat(Sem, RHS.IntVal));
                                 GV.IntVal = apfLHS.bitcastToAPInt();
                                 break;
                         }
                     }
                     break;
                 }
                 return GV;
             }
             default:
                 break;
         }
 
         SmallString<256> Msg;
         raw_svector_ostream OS(Msg);
         OS << "ConstantExpr not handled: " << *CE;
         report_fatal_error(OS.str());
     }
 
     // Otherwise, we have a simple constant.
     GenericValue Result;
     switch (C->getType()->getTypeID())
     {
         case Type::FloatTyID:
             Result.FloatVal = cast<ConstantFP>(C)->getValueAPF().convertToFloat();
             break;
         case Type::X86_FP80TyID:
         {
             auto apf = cast<ConstantFP>(C)->getValueAPF();
             bool lostPrecision;
             apf.convert(APFloat::IEEEdouble(), APFloat::rmNearestTiesToEven, &lostPrecision);
             Result.DoubleVal = apf.convertToDouble();
             break;
         }
         case Type::DoubleTyID:
             Result.DoubleVal = cast<ConstantFP>(C)->getValueAPF().convertToDouble();
             break;
 //		case Type::X86_FP80TyID:
         case Type::FP128TyID:
         case Type::PPC_FP128TyID:
             Result.IntVal = cast <ConstantFP>(C)->getValueAPF().bitcastToAPInt();
             break;
         case Type::IntegerTyID:
             Result.IntVal = cast<ConstantInt>(C)->getValue();
             break;
         case Type::PointerTyID:
             if (isa<ConstantPointerNull>(C))
             {
                 Result.PointerVal = nullptr;
             }
             else if (const Function *F = dyn_cast<Function>(C))
             {
                 //Result = PTOGV(getPointerToFunctionOrStub(const_cast<Function*>(F)));
 
                 // We probably need just any unique value for each function,
                 // so pointer to its LLVM representation should be ok.
                 // But we probably should not need this in our semantics tests,
                 // so we want to know if it ever gets here (assert).
                 assert(false && "taking a pointer to function is not implemented");
                 Result = PTOGV(const_cast<Function*>(F));
             }
             else if (const GlobalVariable *GV = dyn_cast<GlobalVariable>(C))
             {
                 //Result = PTOGV(getOrEmitGlobalVariable(const_cast<GlobalVariable*>(GV)));
 
                 // We probably need just any unique value for each global,
                 // so pointer to its LLVM representation should be ok.
                 // But we probably should not need this in our semantics tests,
                 // so we want to know if it ever gets here (assert).
                 assert(false && "taking a pointer to global variable is not implemented");
                 Result = PTOGV(const_cast<GlobalVariable*>(GV));
             }
             else
             {
                 llvm_unreachable("Unknown constant pointer type!");
             }
             break;
        default:
            if (C->getType()->isVectorTy()) {
                unsigned elemNum;
                Type* ElemTy;
                const ConstantDataVector *CDV = dyn_cast<ConstantDataVector>(C);
                const ConstantVector *CV = dyn_cast<ConstantVector>(C);
                const ConstantAggregateZero *CAZ = dyn_cast<ConstantAggregateZero>(C);

                if (CDV)
                {
                    elemNum = CDV->getNumElements();
                    ElemTy = CDV->getElementType();
                }
                else if (CV || CAZ)
                {
                    VectorType* VTy = dyn_cast<VectorType>(C->getType());
                    elemNum = VTy->getElementCount().getFixedValue();
                    ElemTy = VTy->getElementType();
                }
                else
                {
                    llvm_unreachable("Unknown constant vector type!");
                }

                Result.AggregateVal.resize(elemNum);
                // Check if vector holds floats.
                if(ElemTy->isFloatTy())
                {
                    if (CAZ)
                    {
                        GenericValue floatZero;
                        floatZero.FloatVal = 0.f;
                        std::fill(Result.AggregateVal.begin(), Result.AggregateVal.end(),
                                floatZero);
                        break;
                    }
                    if(CV)
                    {
                        for (unsigned i = 0; i < elemNum; ++i)
                            if (!isa<UndefValue>(CV->getOperand(i)))
                                Result.AggregateVal[i].FloatVal = cast<ConstantFP>(
                                        CV->getOperand(i))->getValueAPF().convertToFloat();
                        break;
                    }
                    if(CDV)
                        for (unsigned i = 0; i < elemNum; ++i)
                            Result.AggregateVal[i].FloatVal = CDV->getElementAsFloat(i);

                    break;
                }
                // Check if vector holds doubles.
                if (ElemTy->isDoubleTy())
                {
                    if (CAZ)
                    {
                        GenericValue doubleZero;
                        doubleZero.DoubleVal = 0.0;
                        std::fill(Result.AggregateVal.begin(), Result.AggregateVal.end(),
                                doubleZero);
                        break;
                    }
                    if(CV)
                    {
                        for (unsigned i = 0; i < elemNum; ++i)
                            if (!isa<UndefValue>(CV->getOperand(i)))
                                Result.AggregateVal[i].DoubleVal = cast<ConstantFP>(
                                        CV->getOperand(i))->getValueAPF().convertToDouble();
                        break;
                    }
                    if(CDV)
                        for (unsigned i = 0; i < elemNum; ++i)
                            Result.AggregateVal[i].DoubleVal = CDV->getElementAsDouble(i);

                    break;
                }
                // Check if vector holds integers.
                if (ElemTy->isIntegerTy())
                {
                    if (CAZ)
                    {
                        GenericValue intZero;
                        intZero.IntVal = APInt(ElemTy->getScalarSizeInBits(), 0ull);
                        std::fill(Result.AggregateVal.begin(), Result.AggregateVal.end(),
                                intZero);
                        break;
                    }
                    if(CV)
                    {
                        for (unsigned i = 0; i < elemNum; ++i)
                            if (!isa<UndefValue>(CV->getOperand(i)))
                                Result.AggregateVal[i].IntVal = cast<ConstantInt>(
                                        CV->getOperand(i))->getValue();
                            else
                            {
                                Result.AggregateVal[i].IntVal =
                                        APInt(CV->getOperand(i)->getType()->getPrimitiveSizeInBits(), 0);
                            }
                        break;
                    }
                    if(CDV)
                        for (unsigned i = 0; i < elemNum; ++i)
                            Result.AggregateVal[i].IntVal = APInt(
                                    CDV->getElementType()->getPrimitiveSizeInBits(),
                                    CDV->getElementAsInteger(i));

                    break;
                }
                break;
            } else {
                SmallString<256> Msg;
                raw_svector_ostream OS(Msg);
                OS << "ERROR: Constant unimplemented for type: " << *C->getType();
                report_fatal_error(OS.str());
            }
     }
 
    return Result;
}

} // llvmir_emul
} // retdec
