/**
 * @file src/llvmir-emul/conversion_instructions.cpp
 * @brief Type conversion and cast instruction implementations.
 * @copyright (c) 2017 Avast Software, licensed under the MIT license
 */

#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/MathExtras.h>

#include "Analysis/llvmir-emul/llvmir_emul.h"

using namespace llvm;

namespace retdec {
namespace llvmir_emul {
 //
 //=============================================================================
 // Conversion Instruction Implementations
 //=============================================================================
 //
 
 GenericValue executeTruncInst(
         Value *SrcVal,
         Type *DstTy,
         LocalExecutionContext &SF,
         GlobalExecutionContext& GC)
 {
     GenericValue Dest, Src = GC.getOperandValue(SrcVal, SF);
     Type *SrcTy = SrcVal->getType();
     if (SrcTy->isVectorTy())
     {
         Type *DstVecTy = DstTy->getScalarType();
         unsigned DBitWidth = cast<IntegerType>(DstVecTy)->getBitWidth();
         unsigned NumElts = Src.AggregateVal.size();
         // the sizes of src and dst vectors must be equal
         Dest.AggregateVal.resize(NumElts);
         for (unsigned i = 0; i < NumElts; i++)
             Dest.AggregateVal[i].IntVal = Src.AggregateVal[i].IntVal.trunc(DBitWidth);
     }
     else
     {
         IntegerType *DITy = cast<IntegerType>(DstTy);
         unsigned DBitWidth = DITy->getBitWidth();
         Dest.IntVal = Src.IntVal.trunc(DBitWidth);
     }
     return Dest;
 }
 
 GenericValue executeSExtInst(
         Value *SrcVal,
         Type *DstTy,
         LocalExecutionContext &SF,
         GlobalExecutionContext& GC)
 {
     Type *SrcTy = SrcVal->getType();
     GenericValue Dest, Src = GC.getOperandValue(SrcVal, SF);
     if (SrcTy->isVectorTy())
     {
         Type *DstVecTy = DstTy->getScalarType();
         unsigned DBitWidth = cast<IntegerType>(DstVecTy)->getBitWidth();
         unsigned size = Src.AggregateVal.size();
         // the sizes of src and dst vectors must be equal.
         Dest.AggregateVal.resize(size);
         for (unsigned i = 0; i < size; i++)
             Dest.AggregateVal[i].IntVal = Src.AggregateVal[i].IntVal.sext(DBitWidth);
     }
     else
     {
         auto *DITy = cast<IntegerType>(DstTy);
         unsigned DBitWidth = DITy->getBitWidth();
         Dest.IntVal = Src.IntVal.sext(DBitWidth);
     }
     return Dest;
 }
 
 GenericValue executeZExtInst(
         Value *SrcVal,
         Type *DstTy,
         LocalExecutionContext &SF,
         GlobalExecutionContext& GC)
 {
     Type *SrcTy = SrcVal->getType();
     GenericValue Dest, Src = GC.getOperandValue(SrcVal, SF);
     if (SrcTy->isVectorTy())
     {
         Type *DstVecTy = DstTy->getScalarType();
         unsigned DBitWidth = cast<IntegerType>(DstVecTy)->getBitWidth();
 
         unsigned size = Src.AggregateVal.size();
         // the sizes of src and dst vectors must be equal.
         Dest.AggregateVal.resize(size);
         for (unsigned i = 0; i < size; i++)
             Dest.AggregateVal[i].IntVal = Src.AggregateVal[i].IntVal.zext(DBitWidth);
     }
     else
     {
         auto *DITy = cast<IntegerType>(DstTy);
         unsigned DBitWidth = DITy->getBitWidth();
         Dest.IntVal = Src.IntVal.zextOrTrunc(DBitWidth);
     }
     return Dest;
 }
 
 GenericValue executeFPTruncInst(
         Value *SrcVal,
         Type *DstTy,
         LocalExecutionContext &SF,
         GlobalExecutionContext& GC)
 {
     GenericValue Dest, Src = GC.getOperandValue(SrcVal, SF);
 
     if (SrcVal->getType()->getTypeID() == Type::VectorTyID)
     {
         assert(SrcVal->getType()->getScalarType()->isDoubleTy() &&
                 DstTy->getScalarType()->isFloatTy() &&
                 "Invalid FPTrunc instruction");
 
         unsigned size = Src.AggregateVal.size();
         // the sizes of src and dst vectors must be equal.
         Dest.AggregateVal.resize(size);
         for (unsigned i = 0; i < size; i++)
         {
             Dest.AggregateVal[i].FloatVal = static_cast<float>(Src.AggregateVal[i].DoubleVal);
         }
     }
     else if (SrcVal->getType()->isDoubleTy() && DstTy->isFloatTy())
     {
         Dest.FloatVal = static_cast<float>(Src.DoubleVal);
     }
     else if (SrcVal->getType()->isX86_FP80Ty() && DstTy->isFloatTy())
     {
         Dest.FloatVal = static_cast<float>(Src.DoubleVal);
     }
     else if (SrcVal->getType()->isX86_FP80Ty() && DstTy->isDoubleTy())
     {
         Dest.DoubleVal = Src.DoubleVal;
     }
     else
     {
         assert(false && "some other type combo");
     }
 
     return Dest;
 }
 
 GenericValue executeFPExtInst(
         Value *SrcVal,
         Type *DstTy,
         LocalExecutionContext &SF,
         GlobalExecutionContext& GC)
 {
     GenericValue Dest, Src = GC.getOperandValue(SrcVal, SF);
 
     if (SrcVal->getType()->getTypeID() == Type::VectorTyID)
     {
         assert(SrcVal->getType()->getScalarType()->isFloatTy() &&
                 DstTy->getScalarType()->isDoubleTy() && "Invalid FPExt instruction");
 
         unsigned size = Src.AggregateVal.size();
         // the sizes of src and dst vectors must be equal.
         Dest.AggregateVal.resize(size);
         for (unsigned i = 0; i < size; i++)
             Dest.AggregateVal[i].DoubleVal = static_cast<double>(Src.AggregateVal[i].FloatVal);
     }
     else if (SrcVal->getType()->isFloatTy()
             && (DstTy->isDoubleTy() || DstTy->isX86_FP80Ty()))
     {
         Dest.DoubleVal = static_cast<double>(Src.FloatVal);
     }
     else if (SrcVal->getType()->isDoubleTy() && DstTy->isX86_FP80Ty())
     {
         Dest.DoubleVal = Src.DoubleVal;
     }
     else
     {
         assert(false && "some other type combo");
     }
 
     return Dest;
 }
 
 GenericValue executeFPToUIInst(
         Value *SrcVal,
         Type *DstTy,
         LocalExecutionContext &SF,
         GlobalExecutionContext& GC)
 {
     Type *SrcTy = SrcVal->getType();
     GenericValue Dest, Src = GC.getOperandValue(SrcVal, SF);
 
     if (SrcTy->getTypeID() == Type::VectorTyID)
     {
         Type *DstVecTy = DstTy->getScalarType();
         Type *SrcVecTy = SrcTy->getScalarType();
         uint32_t DBitWidth = cast<IntegerType>(DstVecTy)->getBitWidth();
         unsigned size = Src.AggregateVal.size();
         // the sizes of src and dst vectors must be equal.
         Dest.AggregateVal.resize(size);
 
         if (SrcVecTy->getTypeID() == Type::FloatTyID)
         {
             assert(SrcVecTy->isFloatingPointTy() && "Invalid FPToUI instruction");
             for (unsigned i = 0; i < size; i++)
                 Dest.AggregateVal[i].IntVal = APIntOps::RoundFloatToAPInt(
                         Src.AggregateVal[i].FloatVal, DBitWidth);
         }
         else
         {
             for (unsigned i = 0; i < size; i++)
                 Dest.AggregateVal[i].IntVal = APIntOps::RoundDoubleToAPInt(
                         Src.AggregateVal[i].DoubleVal, DBitWidth);
         }
     }
     else
     {
         // scalar
         uint32_t DBitWidth = cast<IntegerType>(DstTy)->getBitWidth();
         assert(SrcTy->isFloatingPointTy() && "Invalid FPToUI instruction");
 
         if (SrcTy->getTypeID() == Type::FloatTyID)
         {
             Dest.IntVal = APIntOps::RoundFloatToAPInt(Src.FloatVal, DBitWidth);
         }
         else
         {
             Dest.IntVal = APIntOps::RoundDoubleToAPInt(Src.DoubleVal, DBitWidth);
         }
     }
 
     return Dest;
 }
 
 GenericValue executeFPToSIInst(
         Value *SrcVal,
         Type *DstTy,
         LocalExecutionContext &SF,
         GlobalExecutionContext& GC)
 {
     Type *SrcTy = SrcVal->getType();
     GenericValue Dest, Src = GC.getOperandValue(SrcVal, SF);
 
     if (SrcTy->getTypeID() == Type::VectorTyID)
     {
         Type *DstVecTy = DstTy->getScalarType();
         Type *SrcVecTy = SrcTy->getScalarType();
         uint32_t DBitWidth = cast<IntegerType>(DstVecTy)->getBitWidth();
         unsigned size = Src.AggregateVal.size();
         // the sizes of src and dst vectors must be equal
         Dest.AggregateVal.resize(size);
 
         if (SrcVecTy->getTypeID() == Type::FloatTyID)
         {
             assert(SrcVecTy->isFloatingPointTy() && "Invalid FPToSI instruction");
             for (unsigned i = 0; i < size; i++)
                 Dest.AggregateVal[i].IntVal = APIntOps::RoundFloatToAPInt(
                         Src.AggregateVal[i].FloatVal, DBitWidth);
         }
         else
         {
             for (unsigned i = 0; i < size; i++)
                 Dest.AggregateVal[i].IntVal = APIntOps::RoundDoubleToAPInt(
                         Src.AggregateVal[i].DoubleVal, DBitWidth);
         }
     }
     else
     {
         // scalar
         unsigned DBitWidth = cast<IntegerType>(DstTy)->getBitWidth();
         assert(SrcTy->isFloatingPointTy() && "Invalid FPToSI instruction");
 
         if (SrcTy->getTypeID() == Type::FloatTyID)
         {
             Dest.IntVal = APIntOps::RoundFloatToAPInt(Src.FloatVal, DBitWidth);
         }
         else
         {
             Dest.IntVal = APIntOps::RoundDoubleToAPInt(Src.DoubleVal, DBitWidth);
         }
     }
     return Dest;
 }
 
 GenericValue executeUIToFPInst(
         Value *SrcVal,
         Type *DstTy,
         LocalExecutionContext &SF,
         GlobalExecutionContext& GC)
 {
     GenericValue Dest, Src = GC.getOperandValue(SrcVal, SF);
 
     if (SrcVal->getType()->getTypeID() == Type::VectorTyID)
     {
         Type *DstVecTy = DstTy->getScalarType();
         unsigned size = Src.AggregateVal.size();
         // the sizes of src and dst vectors must be equal
         Dest.AggregateVal.resize(size);
 
         if (DstVecTy->getTypeID() == Type::FloatTyID)
         {
             assert(DstVecTy->isFloatingPointTy()
                     && "Invalid UIToFP instruction");
             for (unsigned i = 0; i < size; i++)
                 Dest.AggregateVal[i].FloatVal = APIntOps::RoundAPIntToFloat(
                         Src.AggregateVal[i].IntVal);
         }
         else
         {
             for (unsigned i = 0; i < size; i++)
                 Dest.AggregateVal[i].DoubleVal = APIntOps::RoundAPIntToDouble(
                         Src.AggregateVal[i].IntVal);
         }
     }
     else
     {
         // scalar
         assert(DstTy->isFloatingPointTy() && "Invalid UIToFP instruction");
         if (DstTy->getTypeID() == Type::FloatTyID)
         {
             Dest.FloatVal = APIntOps::RoundAPIntToFloat(Src.IntVal);
         }
         else
         {
             Dest.DoubleVal = APIntOps::RoundAPIntToDouble(Src.IntVal);
         }
     }
     return Dest;
 }
 
 GenericValue executeSIToFPInst(
         Value *SrcVal,
         Type *DstTy,
         LocalExecutionContext &SF,
         GlobalExecutionContext& GC)
 {
     GenericValue Dest, Src = GC.getOperandValue(SrcVal, SF);
 
     if (SrcVal->getType()->getTypeID() == Type::VectorTyID)
     {
         Type *DstVecTy = DstTy->getScalarType();
         unsigned size = Src.AggregateVal.size();
         // the sizes of src and dst vectors must be equal
         Dest.AggregateVal.resize(size);
 
         if (DstVecTy->getTypeID() == Type::FloatTyID)
         {
             assert(DstVecTy->isFloatingPointTy() && "Invalid SIToFP instruction");
             for (unsigned i = 0; i < size; i++)
                 Dest.AggregateVal[i].FloatVal =
                         APIntOps::RoundSignedAPIntToFloat(Src.AggregateVal[i].IntVal);
         }
         else
         {
             for (unsigned i = 0; i < size; i++)
                 Dest.AggregateVal[i].DoubleVal =
                         APIntOps::RoundSignedAPIntToDouble(Src.AggregateVal[i].IntVal);
         }
     }
     else
     {
         // scalar
         assert(DstTy->isFloatingPointTy() && "Invalid SIToFP instruction");
 
         if (DstTy->getTypeID() == Type::FloatTyID)
         {
             Dest.FloatVal = APIntOps::RoundSignedAPIntToFloat(Src.IntVal);
         }
         else
         {
             Dest.DoubleVal = APIntOps::RoundSignedAPIntToDouble(Src.IntVal);
         }
     }
 
     return Dest;
 }
 
 GenericValue executePtrToIntInst(
         Value *SrcVal,
         Type *DstTy,
         LocalExecutionContext &SF,
         GlobalExecutionContext& GC)
 {
     uint32_t DBitWidth = cast<IntegerType>(DstTy)->getBitWidth();
     GenericValue Dest, Src = GC.getOperandValue(SrcVal, SF);
     assert(SrcVal->getType()->isPointerTy() && "Invalid PtrToInt instruction");
 
     Dest.IntVal = APInt(DBitWidth, reinterpret_cast<intptr_t>(Src.PointerVal));
     return Dest;
 }
 
 GenericValue executeIntToPtrInst(
         Value *SrcVal,
         Type *DstTy,
         LocalExecutionContext &SF,
         GlobalExecutionContext& GC)
 {
     GenericValue Dest, Src = GC.getOperandValue(SrcVal, SF);
     assert(DstTy->isPointerTy() && "Invalid PtrToInt instruction");
 
     uint32_t PtrSize = SF.getModule()->getDataLayout().getPointerSizeInBits();
     if (PtrSize != Src.IntVal.getBitWidth())
     {
         Src.IntVal = Src.IntVal.zextOrTrunc(PtrSize);
     }
 
     Dest.PointerVal = PointerTy(static_cast<intptr_t>(Src.IntVal.getZExtValue()));
     return Dest;
 }
 
 GenericValue executeBitCastInst(
         Value *SrcVal,
         Type *DstTy,
         LocalExecutionContext &SF,
         GlobalExecutionContext& GC)
 {
     // This instruction supports bitwise conversion of vectors to integers and
     // to vectors of other types (as long as they have the same size)
     Type *SrcTy = SrcVal->getType();
     GenericValue Dest, Src = GC.getOperandValue(SrcVal, SF);
 
     if ((SrcTy->getTypeID() == Type::VectorTyID)
             || (DstTy->getTypeID() == Type::VectorTyID))
     {
         // vector src bitcast to vector dst or vector src bitcast to scalar dst or
         // scalar src bitcast to vector dst
         bool isLittleEndian = SF.getModule()->getDataLayout().isLittleEndian();
         GenericValue TempDst, TempSrc, SrcVec;
         Type *SrcElemTy;
         Type *DstElemTy;
         unsigned SrcBitSize;
         unsigned DstBitSize;
         unsigned SrcNum;
         unsigned DstNum;
 
         if (SrcTy->getTypeID() == Type::VectorTyID)
         {
             SrcElemTy = SrcTy->getScalarType();
             SrcBitSize = SrcTy->getScalarSizeInBits();
             SrcNum = Src.AggregateVal.size();
             SrcVec = Src;
         }
         else
         {
             // if src is scalar value, make it vector <1 x type>
             SrcElemTy = SrcTy;
             SrcBitSize = SrcTy->getPrimitiveSizeInBits();
             SrcNum = 1;
             SrcVec.AggregateVal.push_back(Src);
         }
 
         if (DstTy->getTypeID() == Type::VectorTyID)
         {
             DstElemTy = DstTy->getScalarType();
             DstBitSize = DstTy->getScalarSizeInBits();
             DstNum = (SrcNum * SrcBitSize) / DstBitSize;
         }
         else
         {
             DstElemTy = DstTy;
             DstBitSize = DstTy->getPrimitiveSizeInBits();
             DstNum = 1;
         }
 
         if (SrcNum * SrcBitSize != DstNum * DstBitSize)
             llvm_unreachable("Invalid BitCast");
 
         // If src is floating point, cast to integer first.
         TempSrc.AggregateVal.resize(SrcNum);
         if (SrcElemTy->isFloatTy())
         {
             for (unsigned i = 0; i < SrcNum; i++)
                 TempSrc.AggregateVal[i].IntVal = APInt::floatToBits(
                         SrcVec.AggregateVal[i].FloatVal);
 
         }
         else if (SrcElemTy->isDoubleTy() || SrcElemTy->isX86_FP80Ty())
         {
             for (unsigned i = 0; i < SrcNum; i++)
                 TempSrc.AggregateVal[i].IntVal = APInt::doubleToBits(
                         SrcVec.AggregateVal[i].DoubleVal);
         }
         else if (SrcElemTy->isIntegerTy())
         {
             for (unsigned i = 0; i < SrcNum; i++)
                 TempSrc.AggregateVal[i].IntVal = SrcVec.AggregateVal[i].IntVal;
         }
         else
         {
             // Pointers are not allowed as the element type of vector.
             llvm_unreachable("Invalid Bitcast");
         }
 
         // now TempSrc is integer type vector
         if (DstNum < SrcNum)
         {
             // Example: bitcast <4 x i32> <i32 0, i32 1, i32 2, i32 3> to <2 x i64>
             unsigned Ratio = SrcNum / DstNum;
             unsigned SrcElt = 0;
             for (unsigned i = 0; i < DstNum; i++)
             {
                 GenericValue Elt;
                 Elt.IntVal = 0;
                 Elt.IntVal = Elt.IntVal.zext(DstBitSize);
                 unsigned ShiftAmt =
                         isLittleEndian ? 0 : SrcBitSize * (Ratio - 1);
                 for (unsigned j = 0; j < Ratio; j++)
                 {
                     APInt Tmp;
                     Tmp = Tmp.zext(SrcBitSize);
                     Tmp = TempSrc.AggregateVal[SrcElt++].IntVal;
                     Tmp = Tmp.zext(DstBitSize);
                     Tmp = Tmp.shl(ShiftAmt);
                     ShiftAmt += isLittleEndian ? SrcBitSize : -SrcBitSize;
                     Elt.IntVal |= Tmp;
                 }
                 TempDst.AggregateVal.push_back(Elt);
             }
         }
         else
         {
             // Example: bitcast <2 x i64> <i64 0, i64 1> to <4 x i32>
             unsigned Ratio = DstNum / SrcNum;
             for (unsigned i = 0; i < SrcNum; i++)
             {
                 unsigned ShiftAmt =
                         isLittleEndian ? 0 : DstBitSize * (Ratio - 1);
                 for (unsigned j = 0; j < Ratio; j++)
                 {
                     GenericValue Elt;
                     Elt.IntVal = Elt.IntVal.zext(SrcBitSize);
                     Elt.IntVal = TempSrc.AggregateVal[i].IntVal;
                     Elt.IntVal = Elt.IntVal.lshr(ShiftAmt);
                     // it could be DstBitSize == SrcBitSize, so check it
                     if (DstBitSize < SrcBitSize)
                         Elt.IntVal = Elt.IntVal.trunc(DstBitSize);
                     ShiftAmt += isLittleEndian ? DstBitSize : -DstBitSize;
                     TempDst.AggregateVal.push_back(Elt);
                 }
             }
         }
 
         // convert result from integer to specified type
         if (DstTy->getTypeID() == Type::VectorTyID)
         {
             if (DstElemTy->isDoubleTy())
             {
                 Dest.AggregateVal.resize(DstNum);
                 for (unsigned i = 0; i < DstNum; i++)
                     Dest.AggregateVal[i].DoubleVal =
                             TempDst.AggregateVal[i].IntVal.bitsToDouble();
             }
             else if (DstElemTy->isFloatTy())
             {
                 Dest.AggregateVal.resize(DstNum);
                 for (unsigned i = 0; i < DstNum; i++)
                     Dest.AggregateVal[i].FloatVal =
                             TempDst.AggregateVal[i].IntVal.bitsToFloat();
             }
             else
             {
                 Dest = TempDst;
             }
         }
         else
         {
             if (DstElemTy->isDoubleTy())
                 Dest.DoubleVal = TempDst.AggregateVal[0].IntVal.bitsToDouble();
             else if (DstElemTy->isFloatTy())
             {
                 Dest.FloatVal = TempDst.AggregateVal[0].IntVal.bitsToFloat();
             }
             else
             {
                 Dest.IntVal = TempDst.AggregateVal[0].IntVal;
             }
         }
     }
     else
     { //  if ((SrcTy->getTypeID() == Type::VectorTyID) ||
         //     (DstTy->getTypeID() == Type::VectorTyID))
 
         // scalar src bitcast to scalar dst
         if (DstTy->isPointerTy())
         {
             assert(SrcTy->isPointerTy() && "Invalid BitCast");
             Dest.PointerVal = Src.PointerVal;
         }
         else if (DstTy->isIntegerTy())
         {
             if (SrcTy->isFloatTy())
             {
                 Dest.IntVal = APInt::floatToBits(Src.FloatVal);
             }
             // FP128 uses double values.
             else if (SrcTy->isDoubleTy() || SrcTy->isFP128Ty())
             {
                 Dest.IntVal = APInt::doubleToBits(Src.DoubleVal);
             }
             else if (SrcTy->isIntegerTy())
             {
                 Dest.IntVal = Src.IntVal;
             }
             else
             {
                 llvm_unreachable("Invalid BitCast");
             }
         }
         else if (DstTy->isFloatTy())
         {
             if (SrcTy->isIntegerTy())
                 Dest.FloatVal = Src.IntVal.bitsToFloat();
             else
             {
                 Dest.FloatVal = Src.FloatVal;
             }
         }
         // FP128 uses double values.
         else if (DstTy->isDoubleTy() || DstTy->isFP128Ty())
         {
             if (SrcTy->isIntegerTy())
             {
                 Dest.DoubleVal = Src.IntVal.bitsToDouble();
             }
             else
             {
                 Dest.DoubleVal = Src.DoubleVal;
             }
         }
         else
         {
             llvm_unreachable("Invalid Bitcast");
         }
     }
 
     return Dest;
 }
 

} // llvmir_emul
} // retdec
