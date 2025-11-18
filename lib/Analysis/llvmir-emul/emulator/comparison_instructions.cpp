/**
 * @file src/llvmir-emul/comparison_instructions.cpp
 * @brief Comparison instruction implementations (ICMP and FCMP).
 */

#include <llvm/IR/Type.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/raw_ostream.h>

#include "Analysis/llvmir-emul/llvmir_emul.h"

using namespace llvm;

namespace retdec {
namespace llvmir_emul {
 #define IMPLEMENT_INTEGER_ICMP(OP, TY) \
     case Type::IntegerTyID:  \
         Dest.IntVal = APInt(1,Src1.IntVal.OP(Src2.IntVal)); \
         break;
 
#define IMPLEMENT_VECTOR_INTEGER_ICMP(OP, TY)                              \
    {                                                                      \
        assert(Src1.AggregateVal.size() == Src2.AggregateVal.size());      \
        Dest.AggregateVal.resize( Src1.AggregateVal.size() );              \
        for(uint32_t _i=0;_i<Src1.AggregateVal.size();_i++)                \
            Dest.AggregateVal[_i].IntVal = APInt(1,                        \
            Src1.AggregateVal[_i].IntVal.OP(Src2.AggregateVal[_i].IntVal));\
    }
 
 // Handle pointers specially because they must be compared with only as much
 // width as the host has.  We _do not_ want to be comparing 64 bit values when
 // running on a 32-bit target, otherwise the upper 32 bits might mess up
 // comparisons if they contain garbage.
 // Matula: This may not be the case for emulation, but it will probable be ok.
 #define IMPLEMENT_POINTER_ICMP(OP) \
     case Type::PointerTyID: \
         Dest.IntVal = APInt(1, reinterpret_cast<void*>(reinterpret_cast<intptr_t>(Src1.PointerVal)) OP \
                 reinterpret_cast<void*>(reinterpret_cast<intptr_t>(Src2.PointerVal))); \
         break;
 
GenericValue executeICMP_EQ(
        GenericValue Src1,
        GenericValue Src2,
        Type *Ty)
{
    GenericValue Dest;
    if (Ty->isVectorTy()) {
        IMPLEMENT_VECTOR_INTEGER_ICMP(eq,Ty);
    } else {
        switch (Ty->getTypeID())
        {
            IMPLEMENT_INTEGER_ICMP(eq,Ty);
            IMPLEMENT_POINTER_ICMP(==);
            default:
                dbgs() << "Unhandled type for ICMP_EQ predicate: " << *Ty << "\n";
                llvm_unreachable(nullptr);
        }
    }
    return Dest;
}
 
 GenericValue executeICMP_NE(
         GenericValue Src1,
         GenericValue Src2,
         Type *Ty)
 {
     GenericValue Dest;
     if (Ty->isVectorTy()) {
         IMPLEMENT_VECTOR_INTEGER_ICMP(ne,Ty);
     } else {
         switch (Ty->getTypeID())
         {
             IMPLEMENT_INTEGER_ICMP(ne,Ty);
             IMPLEMENT_POINTER_ICMP(!=);
             default:
                 dbgs() << "Unhandled type for ICMP_NE predicate: " << *Ty << "\n";
                 llvm_unreachable(nullptr);
         }
     }
     return Dest;
 }
 
 GenericValue executeICMP_ULT(
         GenericValue Src1,
         GenericValue Src2,
         Type *Ty)
 {
     GenericValue Dest;
     if (Ty->isVectorTy()) {
         IMPLEMENT_VECTOR_INTEGER_ICMP(ult,Ty);
     } else {
         switch (Ty->getTypeID())
         {
             IMPLEMENT_INTEGER_ICMP(ult,Ty);
             IMPLEMENT_POINTER_ICMP(<);
             default:
                 dbgs() << "Unhandled type for ICMP_ULT predicate: " << *Ty << "\n";
                 llvm_unreachable(nullptr);
         }
     }
     return Dest;
 }
 
 GenericValue executeICMP_SLT(
         GenericValue Src1,
         GenericValue Src2,
         Type *Ty)
 {
     GenericValue Dest;
     if (Ty->isVectorTy()) {
         IMPLEMENT_VECTOR_INTEGER_ICMP(slt,Ty);
     } else {
         switch (Ty->getTypeID())
         {
             IMPLEMENT_INTEGER_ICMP(slt,Ty);
             IMPLEMENT_POINTER_ICMP(<);
             default:
                 dbgs() << "Unhandled type for ICMP_SLT predicate: " << *Ty << "\n";
                 llvm_unreachable(nullptr);
         }
     }
     return Dest;
 }
 
 GenericValue executeICMP_UGT(
         GenericValue Src1,
         GenericValue Src2,
         Type *Ty)
 {
     GenericValue Dest;
     if (Ty->isVectorTy()) {
         IMPLEMENT_VECTOR_INTEGER_ICMP(ugt,Ty);
     } else {
         switch (Ty->getTypeID())
         {
             IMPLEMENT_INTEGER_ICMP(ugt,Ty);
             IMPLEMENT_POINTER_ICMP(>);
             default:
                 dbgs() << "Unhandled type for ICMP_UGT predicate: " << *Ty << "\n";
                 llvm_unreachable(nullptr);
         }
     }
     return Dest;
 }
 
 GenericValue executeICMP_SGT(
         GenericValue Src1,
         GenericValue Src2,
         Type *Ty)
 {
     GenericValue Dest;
     if (Ty->isVectorTy()) {
         IMPLEMENT_VECTOR_INTEGER_ICMP(sgt,Ty);
     } else {
         switch (Ty->getTypeID())
         {
             IMPLEMENT_INTEGER_ICMP(sgt,Ty);
             IMPLEMENT_POINTER_ICMP(>);
             default:
                 dbgs() << "Unhandled type for ICMP_SGT predicate: " << *Ty << "\n";
                 llvm_unreachable(nullptr);
         }
     }
     return Dest;
 }
 
 GenericValue executeICMP_ULE(
         GenericValue Src1,
         GenericValue Src2,
         Type *Ty)
 {
     GenericValue Dest;
     if (Ty->isVectorTy()) {
         IMPLEMENT_VECTOR_INTEGER_ICMP(ule,Ty);
     } else {
         switch (Ty->getTypeID())
         {
             IMPLEMENT_INTEGER_ICMP(ule,Ty);
             IMPLEMENT_POINTER_ICMP(<=);
             default:
                 dbgs() << "Unhandled type for ICMP_ULE predicate: " << *Ty << "\n";
                 llvm_unreachable(nullptr);
         }
     }
     return Dest;
 }
 
 GenericValue executeICMP_SLE(
         GenericValue Src1,
         GenericValue Src2,
         Type *Ty)
 {
     GenericValue Dest;
     if (Ty->isVectorTy()) {
         IMPLEMENT_VECTOR_INTEGER_ICMP(sle,Ty);
     } else {
         switch (Ty->getTypeID())
         {
             IMPLEMENT_INTEGER_ICMP(sle,Ty);
             IMPLEMENT_POINTER_ICMP(<=);
             default:
                 dbgs() << "Unhandled type for ICMP_SLE predicate: " << *Ty << "\n";
                 llvm_unreachable(nullptr);
         }
     }
     return Dest;
 }
 
 GenericValue executeICMP_UGE(
         GenericValue Src1,
         GenericValue Src2,
         Type *Ty)
 {
     GenericValue Dest;
     if (Ty->isVectorTy()) {
         IMPLEMENT_VECTOR_INTEGER_ICMP(uge,Ty);
     } else {
         switch (Ty->getTypeID())
         {
             IMPLEMENT_INTEGER_ICMP(uge,Ty);
             IMPLEMENT_POINTER_ICMP(>=);
             default:
                 dbgs() << "Unhandled type for ICMP_UGE predicate: " << *Ty << "\n";
                 llvm_unreachable(nullptr);
         }
     }
     return Dest;
 }
 
 GenericValue executeICMP_SGE(
         GenericValue Src1,
         GenericValue Src2,
         Type *Ty)
 {
     GenericValue Dest;
    if (Ty->isVectorTy()) {
        IMPLEMENT_VECTOR_INTEGER_ICMP(sge,Ty);
    } else {
        switch (Ty->getTypeID())
        {
            IMPLEMENT_INTEGER_ICMP(sge,Ty);
            IMPLEMENT_POINTER_ICMP(>=);
            default:
                dbgs() << "Unhandled type for ICMP_SGE predicate: " << *Ty << "\n";
                llvm_unreachable(nullptr);
        }
    }
     return Dest;
 }
 
 #define IMPLEMENT_FCMP(OP, TY) \
     case Type::TY##TyID: \
         Dest.IntVal = APInt(1,Src1.TY##Val OP Src2.TY##Val); \
         break
 
 #define IMPLEMENT_VECTOR_FCMP_T(OP, TY)                                 \
     assert(Src1.AggregateVal.size() == Src2.AggregateVal.size());       \
     Dest.AggregateVal.resize( Src1.AggregateVal.size() );               \
     for( uint32_t _i=0;_i<Src1.AggregateVal.size();_i++)                \
         Dest.AggregateVal[_i].IntVal = APInt(1,                         \
         Src1.AggregateVal[_i].TY##Val OP Src2.AggregateVal[_i].TY##Val);
 
#define IMPLEMENT_VECTOR_FCMP(OP)                                   \
    {                                                               \
    if (cast<VectorType>(Ty)->getElementType()->isFloatTy())        \
    {                                                               \
        IMPLEMENT_VECTOR_FCMP_T(OP, Float);                         \
    }                                                               \
    else                                                            \
    {                                                               \
        IMPLEMENT_VECTOR_FCMP_T(OP, Double);                        \
    }                                                               \
    }
 
 GenericValue executeFCMP_OEQ(
         GenericValue Src1,
         GenericValue Src2,
         Type *Ty)
 {
     GenericValue Dest;
     if (Ty->isVectorTy()) {
         IMPLEMENT_VECTOR_FCMP(==);
     } else {
         switch (Ty->getTypeID())
         {
             IMPLEMENT_FCMP(==, Float);
             case Type::X86_FP80TyID:
             IMPLEMENT_FCMP(==, Double);
             default:
                 dbgs() << "Unhandled type for FCmp EQ instruction: " << *Ty << "\n";
                 llvm_unreachable(nullptr);
         }
     }
     return Dest;
 }
 
 #define IMPLEMENT_SCALAR_NANS(TY, X,Y)                                \
     if (TY->isFloatTy())                                              \
     {                                                                 \
         if (X.FloatVal != X.FloatVal || Y.FloatVal != Y.FloatVal)     \
         {                                                             \
             Dest.IntVal = APInt(1,false);                             \
             return Dest;                                              \
         }                                                             \
     }                                                                 \
     else                                                              \
     {                                                                 \
         if (X.DoubleVal != X.DoubleVal || Y.DoubleVal != Y.DoubleVal) \
         {                                                             \
             Dest.IntVal = APInt(1,false);                             \
             return Dest;                                              \
         }                                                             \
     }
 
 #define MASK_VECTOR_NANS_T(X,Y, TZ, FLAG)                                 \
     assert(X.AggregateVal.size() == Y.AggregateVal.size());               \
     Dest.AggregateVal.resize( X.AggregateVal.size() );                    \
     for( uint32_t _i=0;_i<X.AggregateVal.size();_i++)                     \
     {                                                                     \
         if (X.AggregateVal[_i].TZ##Val != X.AggregateVal[_i].TZ##Val ||   \
                 Y.AggregateVal[_i].TZ##Val != Y.AggregateVal[_i].TZ##Val) \
                 Dest.AggregateVal[_i].IntVal = APInt(1,FLAG);             \
         else                                                              \
         {                                                                 \
             Dest.AggregateVal[_i].IntVal = APInt(1,!FLAG);                \
         }                                                                 \
     }
 
 #define MASK_VECTOR_NANS(TY, X,Y, FLAG)                                \
     if (TY->isVectorTy())                                              \
     {                                                                  \
         if (cast<VectorType>(TY)->getElementType()->isFloatTy())       \
         {                                                              \
             MASK_VECTOR_NANS_T(X, Y, Float, FLAG)                      \
         }                                                              \
         else                                                           \
         {                                                              \
             MASK_VECTOR_NANS_T(X, Y, Double, FLAG)                     \
         }                                                              \
     }
 
 GenericValue executeFCMP_ONE(
         GenericValue Src1,
         GenericValue Src2,
         Type *Ty)
 {
     GenericValue Dest;
     // if input is scalar value and Src1 or Src2 is NaN return false
     IMPLEMENT_SCALAR_NANS(Ty, Src1, Src2)
     // if vector input detect NaNs and fill mask
     MASK_VECTOR_NANS(Ty, Src1, Src2, false)
     GenericValue DestMask = Dest;
     if (Ty->isVectorTy()) {
         IMPLEMENT_VECTOR_FCMP(!=);
     } else {
         switch (Ty->getTypeID())
         {
             IMPLEMENT_FCMP(!=, Float);
             case Type::X86_FP80TyID:
             IMPLEMENT_FCMP(!=, Double);
             default:
                 dbgs() << "Unhandled type for FCmp NE instruction: " << *Ty << "\n";
                 llvm_unreachable(nullptr);
         }
     }
     // in vector case mask out NaN elements
     if (Ty->isVectorTy())
         for( size_t _i=0; _i<Src1.AggregateVal.size(); _i++)
             if (DestMask.AggregateVal[_i].IntVal == false)
                 Dest.AggregateVal[_i].IntVal = APInt(1,false);

     return Dest;
 }
 
 GenericValue executeFCMP_OLE(
         GenericValue Src1,
         GenericValue Src2,
         Type *Ty)
 {
     GenericValue Dest;
     if (Ty->isVectorTy()) {
         IMPLEMENT_VECTOR_FCMP(<=);
     } else {
         switch (Ty->getTypeID())
         {
             IMPLEMENT_FCMP(<=, Float);
             case Type::X86_FP80TyID:
             IMPLEMENT_FCMP(<=, Double);
             default:
                 dbgs() << "Unhandled type for FCmp LE instruction: " << *Ty << "\n";
                 llvm_unreachable(nullptr);
         }
     }
     return Dest;
 }
 
 GenericValue executeFCMP_OGE(
         GenericValue Src1,
         GenericValue Src2,
         Type *Ty)
 {
     GenericValue Dest;
     if (Ty->isVectorTy()) {
         IMPLEMENT_VECTOR_FCMP(>=);
     } else {
         switch (Ty->getTypeID())
         {
             IMPLEMENT_FCMP(>=, Float);
             case Type::X86_FP80TyID:
             IMPLEMENT_FCMP(>=, Double);
             default:
                 dbgs() << "Unhandled type for FCmp GE instruction: " << *Ty << "\n";
                 llvm_unreachable(nullptr);
         }
     }
     return Dest;
 }
 
 GenericValue executeFCMP_OLT(
         GenericValue Src1,
         GenericValue Src2,
         Type *Ty)
 {
     GenericValue Dest;
     if (Ty->isVectorTy()) {
         IMPLEMENT_VECTOR_FCMP(<);
     } else {
         switch (Ty->getTypeID())
         {
             IMPLEMENT_FCMP(<, Float);
             case Type::X86_FP80TyID:
             IMPLEMENT_FCMP(<, Double);
             default:
                 dbgs() << "Unhandled type for FCmp LT instruction: " << *Ty << "\n";
                 llvm_unreachable(nullptr);
         }
     }
     return Dest;
 }
 
 GenericValue executeFCMP_OGT(
         GenericValue Src1,
         GenericValue Src2,
         Type *Ty)
 {
     GenericValue Dest;
     if (Ty->isVectorTy()) {
         IMPLEMENT_VECTOR_FCMP(>);
     } else {
         switch (Ty->getTypeID())
         {
             IMPLEMENT_FCMP(>, Float);
             case Type::X86_FP80TyID:
             IMPLEMENT_FCMP(>, Double);
             default:
                 dbgs() << "Unhandled type for FCmp GT instruction: " << *Ty << "\n";
                 llvm_unreachable(nullptr);
         }
     }
     return Dest;
 }
 
 #define IMPLEMENT_UNORDERED(TY, X,Y)                                         \
     if (TY->isFloatTy())                                                     \
     {                                                                        \
         if (X.FloatVal != X.FloatVal || Y.FloatVal != Y.FloatVal)            \
         {                                                                    \
             Dest.IntVal = APInt(1,true);                                     \
             return Dest;                                                     \
         }                                                                    \
     } else if (X.DoubleVal != X.DoubleVal || Y.DoubleVal != Y.DoubleVal)     \
     {                                                                        \
         Dest.IntVal = APInt(1,true);                                         \
         return Dest;                                                         \
     }
 
 #define IMPLEMENT_VECTOR_UNORDERED(TY, X, Y, FUNC)                           \
     if (TY->isVectorTy())                                                    \
     {                                                                        \
         GenericValue DestMask = Dest;                                        \
         Dest = FUNC(Src1, Src2, Ty);                                         \
         for (size_t _i = 0; _i < Src1.AggregateVal.size(); _i++)             \
             if (DestMask.AggregateVal[_i].IntVal == true)                    \
                 Dest.AggregateVal[_i].IntVal = APInt(1, true);               \
         return Dest;                                                         \
     }
 
 GenericValue executeFCMP_UEQ(
         GenericValue Src1,
         GenericValue Src2,
         Type *Ty)
 {
     GenericValue Dest;
     IMPLEMENT_UNORDERED(Ty, Src1, Src2)
     MASK_VECTOR_NANS(Ty, Src1, Src2, true)
     IMPLEMENT_VECTOR_UNORDERED(Ty, Src1, Src2, executeFCMP_OEQ)
     return executeFCMP_OEQ(Src1, Src2, Ty);
 }
 
 GenericValue executeFCMP_UNE(
         GenericValue Src1,
         GenericValue Src2,
         Type *Ty)
 {
     GenericValue Dest;
     IMPLEMENT_UNORDERED(Ty, Src1, Src2)
     MASK_VECTOR_NANS(Ty, Src1, Src2, true)
     IMPLEMENT_VECTOR_UNORDERED(Ty, Src1, Src2, executeFCMP_ONE)
     return executeFCMP_ONE(Src1, Src2, Ty);
 }
 
 GenericValue executeFCMP_ULE(
         GenericValue Src1,
         GenericValue Src2,
         Type *Ty)
 {
     GenericValue Dest;
     IMPLEMENT_UNORDERED(Ty, Src1, Src2)
     MASK_VECTOR_NANS(Ty, Src1, Src2, true)
     IMPLEMENT_VECTOR_UNORDERED(Ty, Src1, Src2, executeFCMP_OLE)
     return executeFCMP_OLE(Src1, Src2, Ty);
 }
 
 GenericValue executeFCMP_UGE(
         GenericValue Src1,
         GenericValue Src2,
         Type *Ty)
 {
     GenericValue Dest;
     IMPLEMENT_UNORDERED(Ty, Src1, Src2)
     MASK_VECTOR_NANS(Ty, Src1, Src2, true)
     IMPLEMENT_VECTOR_UNORDERED(Ty, Src1, Src2, executeFCMP_OGE)
     return executeFCMP_OGE(Src1, Src2, Ty);
 }
 
 GenericValue executeFCMP_ULT(
         GenericValue Src1,
         GenericValue Src2,
         Type *Ty)
 {
     GenericValue Dest;
     IMPLEMENT_UNORDERED(Ty, Src1, Src2)
     MASK_VECTOR_NANS(Ty, Src1, Src2, true)
     IMPLEMENT_VECTOR_UNORDERED(Ty, Src1, Src2, executeFCMP_OLT)
     return executeFCMP_OLT(Src1, Src2, Ty);
 }
 
 GenericValue executeFCMP_UGT(
         GenericValue Src1,
         GenericValue Src2,
         Type *Ty)
 {
     GenericValue Dest;
     IMPLEMENT_UNORDERED(Ty, Src1, Src2)
     MASK_VECTOR_NANS(Ty, Src1, Src2, true)
     IMPLEMENT_VECTOR_UNORDERED(Ty, Src1, Src2, executeFCMP_OGT)
     return executeFCMP_OGT(Src1, Src2, Ty);
 }
 
 GenericValue executeFCMP_ORD(
         GenericValue Src1,
         GenericValue Src2,
         Type *Ty)
 {
     GenericValue Dest;
     if(Ty->isVectorTy())
     {
         assert(Src1.AggregateVal.size() == Src2.AggregateVal.size());
         Dest.AggregateVal.resize( Src1.AggregateVal.size() );
         if (cast<VectorType>(Ty)->getElementType()->isFloatTy())
         {
             for( size_t _i=0;_i<Src1.AggregateVal.size();_i++)
                 Dest.AggregateVal[_i].IntVal = APInt(
                         1,
                         ( (Src1.AggregateVal[_i].FloatVal ==
                         Src1.AggregateVal[_i].FloatVal) &&
                         (Src2.AggregateVal[_i].FloatVal ==
                         Src2.AggregateVal[_i].FloatVal)));
         }
         else
         {
             for( size_t _i=0;_i<Src1.AggregateVal.size();_i++)
                 Dest.AggregateVal[_i].IntVal = APInt(
                         1,
                         ( (Src1.AggregateVal[_i].DoubleVal ==
                         Src1.AggregateVal[_i].DoubleVal) &&
                         (Src2.AggregateVal[_i].DoubleVal ==
                         Src2.AggregateVal[_i].DoubleVal)));
         }
     }
     else if (Ty->isFloatTy())
     {
         Dest.IntVal = APInt(1,(Src1.FloatVal == Src1.FloatVal &&
                 Src2.FloatVal == Src2.FloatVal));
     }
     else
     {
         Dest.IntVal = APInt(1,(Src1.DoubleVal == Src1.DoubleVal &&
                 Src2.DoubleVal == Src2.DoubleVal));
     }
     return Dest;
 }
 
 GenericValue executeFCMP_UNO(
         GenericValue Src1,
         GenericValue Src2,
         Type *Ty)
 {
     GenericValue Dest;
     if(Ty->isVectorTy())
     {
         assert(Src1.AggregateVal.size() == Src2.AggregateVal.size());
         Dest.AggregateVal.resize( Src1.AggregateVal.size() );
         if (cast<VectorType>(Ty)->getElementType()->isFloatTy())
         {
             for( size_t _i=0;_i<Src1.AggregateVal.size();_i++)
                 Dest.AggregateVal[_i].IntVal = APInt(
                         1,
                         ( (Src1.AggregateVal[_i].FloatVal !=
                         Src1.AggregateVal[_i].FloatVal) ||
                         (Src2.AggregateVal[_i].FloatVal !=
                         Src2.AggregateVal[_i].FloatVal)));
         }
         else
         {
             for( size_t _i=0;_i<Src1.AggregateVal.size();_i++)
                 Dest.AggregateVal[_i].IntVal = APInt(1,
                         ( (Src1.AggregateVal[_i].DoubleVal !=
                         Src1.AggregateVal[_i].DoubleVal) ||
                         (Src2.AggregateVal[_i].DoubleVal !=
                         Src2.AggregateVal[_i].DoubleVal)));
         }
     }
     else if (Ty->isFloatTy())
     {
         Dest.IntVal = APInt(1,(Src1.FloatVal != Src1.FloatVal ||
                 Src2.FloatVal != Src2.FloatVal));
     }
     else
     {
         Dest.IntVal = APInt(1,(Src1.DoubleVal != Src1.DoubleVal ||
                 Src2.DoubleVal != Src2.DoubleVal));
     }
     return Dest;
 }
 
 GenericValue executeFCMP_BOOL(
         GenericValue Src1,
         GenericValue Src2,
         Type *Ty,
         const bool val)
 {
     GenericValue Dest;
     if(Ty->isVectorTy())
     {
         assert(Src1.AggregateVal.size() == Src2.AggregateVal.size());
         Dest.AggregateVal.resize( Src1.AggregateVal.size() );
         for( size_t _i=0; _i<Src1.AggregateVal.size(); _i++)
         {
             Dest.AggregateVal[_i].IntVal = APInt(1,val);
         }
     }
     else
     {
         Dest.IntVal = APInt(1, val);
     }

     return Dest;
 }
 
 GenericValue executeCmpInst(
         unsigned predicate,
         GenericValue Src1,
         GenericValue Src2,
         Type *Ty)
 {
     GenericValue Result;
     switch (predicate)
     {
         case ICmpInst::ICMP_EQ:    return executeICMP_EQ(Src1, Src2, Ty);
         case ICmpInst::ICMP_NE:    return executeICMP_NE(Src1, Src2, Ty);
         case ICmpInst::ICMP_UGT:   return executeICMP_UGT(Src1, Src2, Ty);
         case ICmpInst::ICMP_SGT:   return executeICMP_SGT(Src1, Src2, Ty);
         case ICmpInst::ICMP_ULT:   return executeICMP_ULT(Src1, Src2, Ty);
         case ICmpInst::ICMP_SLT:   return executeICMP_SLT(Src1, Src2, Ty);
         case ICmpInst::ICMP_UGE:   return executeICMP_UGE(Src1, Src2, Ty);
         case ICmpInst::ICMP_SGE:   return executeICMP_SGE(Src1, Src2, Ty);
         case ICmpInst::ICMP_ULE:   return executeICMP_ULE(Src1, Src2, Ty);
         case ICmpInst::ICMP_SLE:   return executeICMP_SLE(Src1, Src2, Ty);
         case FCmpInst::FCMP_ORD:   return executeFCMP_ORD(Src1, Src2, Ty);
         case FCmpInst::FCMP_UNO:   return executeFCMP_UNO(Src1, Src2, Ty);
         case FCmpInst::FCMP_OEQ:   return executeFCMP_OEQ(Src1, Src2, Ty);
         case FCmpInst::FCMP_UEQ:   return executeFCMP_UEQ(Src1, Src2, Ty);
         case FCmpInst::FCMP_ONE:   return executeFCMP_ONE(Src1, Src2, Ty);
         case FCmpInst::FCMP_UNE:   return executeFCMP_UNE(Src1, Src2, Ty);
         case FCmpInst::FCMP_OLT:   return executeFCMP_OLT(Src1, Src2, Ty);
         case FCmpInst::FCMP_ULT:   return executeFCMP_ULT(Src1, Src2, Ty);
         case FCmpInst::FCMP_OGT:   return executeFCMP_OGT(Src1, Src2, Ty);
         case FCmpInst::FCMP_UGT:   return executeFCMP_UGT(Src1, Src2, Ty);
         case FCmpInst::FCMP_OLE:   return executeFCMP_OLE(Src1, Src2, Ty);
         case FCmpInst::FCMP_ULE:   return executeFCMP_ULE(Src1, Src2, Ty);
         case FCmpInst::FCMP_OGE:   return executeFCMP_OGE(Src1, Src2, Ty);
         case FCmpInst::FCMP_UGE:   return executeFCMP_UGE(Src1, Src2, Ty);
         case FCmpInst::FCMP_FALSE: return executeFCMP_BOOL(Src1, Src2, Ty, false);
         case FCmpInst::FCMP_TRUE:  return executeFCMP_BOOL(Src1, Src2, Ty, true);
         default:
             dbgs() << "Unhandled Cmp predicate\n";
             llvm_unreachable(nullptr);
     }
     return Result;
 }

} // llvmir_emul
} // retdec
