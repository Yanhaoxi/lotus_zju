/**
 * @file src/llvmir-emul/binary_instructions.cpp
 * @brief Binary floating point instruction implementations.
 */

#include <llvm/IR/Type.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/raw_ostream.h>
#include <cmath>

#include "Analysis/llvmir-emul/llvmir_emul.h"

using namespace llvm;

namespace retdec {
namespace llvmir_emul {

#define IMPLEMENT_BINARY_OPERATOR(OP, TY) \
    case Type::TY##TyID: \
        Dest.TY##Val = Src1.TY##Val OP Src2.TY##Val; \
        break

void executeFAddInst(
        GenericValue &Dest,
        GenericValue Src1,
        GenericValue Src2,
        Type *Ty)
{
    switch (Ty->getTypeID())
    {
        IMPLEMENT_BINARY_OPERATOR(+, Float);
        case Type::X86_FP80TyID:
        IMPLEMENT_BINARY_OPERATOR(+, Double);
        default:
            dbgs() << "Unhandled type for FAdd instruction: " << *Ty << "\n";
            llvm_unreachable(nullptr);
    }
}

void executeFSubInst(
        GenericValue &Dest,
        GenericValue Src1,
        GenericValue Src2,
        Type *Ty)
{
    switch (Ty->getTypeID())
    {
        IMPLEMENT_BINARY_OPERATOR(-, Float);
        case Type::X86_FP80TyID:
        IMPLEMENT_BINARY_OPERATOR(-, Double);
        default:
            dbgs() << "Unhandled type for FSub instruction: " << *Ty << "\n";
            llvm_unreachable(nullptr);
    }
}

void executeFMulInst(
        GenericValue &Dest,
        GenericValue Src1,
        GenericValue Src2,
        Type *Ty)
{
    switch (Ty->getTypeID())
    {
        IMPLEMENT_BINARY_OPERATOR(*, Float);
        case Type::X86_FP80TyID:
        IMPLEMENT_BINARY_OPERATOR(*, Double);
        default:
            dbgs() << "Unhandled type for FMul instruction: " << *Ty << "\n";
            llvm_unreachable(nullptr);
    }
}

void executeFDivInst(
        GenericValue &Dest,
        GenericValue Src1,
        GenericValue Src2,
        Type *Ty)
{
    switch (Ty->getTypeID())
    {
        IMPLEMENT_BINARY_OPERATOR(/, Float);
        case Type::X86_FP80TyID:
        IMPLEMENT_BINARY_OPERATOR(/, Double);
        default:
            dbgs() << "Unhandled type for FDiv instruction: " << *Ty << "\n";
            llvm_unreachable(nullptr);
    }
}

void executeFRemInst(
        GenericValue &Dest,
        GenericValue Src1,
        GenericValue Src2,
        Type *Ty)
{
    switch (Ty->getTypeID())
    {
        case Type::FloatTyID:
            Dest.FloatVal = fmod(Src1.FloatVal, Src2.FloatVal);
            break;
        case Type::X86_FP80TyID:
        case Type::DoubleTyID:
            Dest.DoubleVal = fmod(Src1.DoubleVal, Src2.DoubleVal);
            break;
        default:
            dbgs() << "Unhandled type for Rem instruction: " << *Ty << "\n";
            llvm_unreachable(nullptr);
    }
}

} // llvmir_emul
} // retdec

