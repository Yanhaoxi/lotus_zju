/**
 * @file src/llvmir-emul/helper_functions.cpp
 * @brief Helper functions for GEP, select, and basic block operations.
 * @copyright (c) 2017 Avast Software, licensed under the MIT license
 */

#include <llvm/IR/Type.h>
#include <llvm/IR/GetElementPtrTypeIterator.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/raw_ostream.h>

#include "Analysis/llvmir-emul/llvmir_emul.h"

using namespace llvm;

namespace retdec {
namespace llvmir_emul {
 GenericValue executeSelectInst(
         GenericValue Src1,
         GenericValue Src2,
         GenericValue Src3,
         Type *Ty)
 {
     GenericValue Dest;
     if(Ty->isVectorTy())
     {
         assert(Src1.AggregateVal.size() == Src2.AggregateVal.size());
         assert(Src2.AggregateVal.size() == Src3.AggregateVal.size());
         Dest.AggregateVal.resize( Src1.AggregateVal.size() );
         for (size_t i = 0; i < Src1.AggregateVal.size(); ++i)
             Dest.AggregateVal[i] = (Src1.AggregateVal[i].IntVal == 0) ?
                     Src3.AggregateVal[i] : Src2.AggregateVal[i];
     }
     else
     {
         Dest = (Src1.IntVal == 0) ? Src3 : Src2;
     }
     return Dest;
 }
 
 //
 //=============================================================================
 // Terminator Instruction Implementations
 //=============================================================================
 //
 
 // switchToNewBasicBlock - This method is used to jump to a new basic block.
 // This function handles the actual updating of block and instruction iterators
 // as well as execution of all of the PHI nodes in the destination block.
 //
 // This method does this because all of the PHI nodes must be executed
 // atomically, reading their inputs before any of the results are updated.  Not
 // doing this can cause problems if the PHI nodes depend on other PHI nodes for
 // their inputs.  If the input PHI node is updated before it is read, incorrect
 // results can happen.  Thus we use a two phase approach.
 //
 void switchToNewBasicBlock(
         BasicBlock* Dest,
         LocalExecutionContext& SF,
         GlobalExecutionContext& GC)
 {
     BasicBlock *PrevBB = SF.curBB;      // Remember where we came from...
     SF.curBB   = Dest;                  // Update CurBB to branch destination
     SF.curInst = SF.curBB->begin();     // Update new instruction ptr...
 
     if (!isa<PHINode>(SF.curInst))
     {
         return;  // Nothing fancy to do
     }
 
     // Loop over all of the PHI nodes in the current block, reading their inputs.
     std::vector<GenericValue> ResultValues;
 
     for (; PHINode *PN = dyn_cast<PHINode>(SF.curInst); ++SF.curInst)
     {
         // Search for the value corresponding to this previous bb...
         int i = PN->getBasicBlockIndex(PrevBB);
         assert(i != -1 && "PHINode doesn't contain entry for predecessor??");
         Value *IncomingValue = PN->getIncomingValue(i);
 
         // Save the incoming value for this PHI node...
         ResultValues.push_back(GC.getOperandValue(IncomingValue, SF));
     }
 
     // Now loop over all of the PHI nodes setting their values...
     SF.curInst = SF.curBB->begin();
     for (unsigned i = 0; isa<PHINode>(SF.curInst); ++SF.curInst, ++i)
     {
         PHINode *PN = cast<PHINode>(SF.curInst);
         GC.setValue(PN, ResultValues[i]);
     }
 }
 
 //
 //=============================================================================
 // Memory Instruction Implementations
 //=============================================================================
 //
 
 /**
  * getElementOffset - The workhorse for getelementptr.
  */
 GenericValue executeGEPOperation(
         Value *Ptr,
         gep_type_iterator I,
         gep_type_iterator E,
         LocalExecutionContext& SF,
         GlobalExecutionContext& GC)
 {
     assert(Ptr->getType()->isPointerTy()
             && "Cannot getElementOffset of a nonpointer type!");
 
     auto& DL = GC.getModule()->getDataLayout();
 
     uint64_t Total = 0;
 
     for (; I != E; ++I)
     {
         if (StructType *STy = I.getStructTypeOrNull())
         {
             const StructLayout *SLO = DL.getStructLayout(STy);
 
             const ConstantInt *CPU = cast<ConstantInt>(I.getOperand());
             unsigned Index = unsigned(CPU->getZExtValue());
 
             Total += SLO->getElementOffset(Index);
         }
         else
         {
             // Get the index number for the array... which must be long type...
             GenericValue IdxGV = GC.getOperandValue(I.getOperand(), SF);
 
             int64_t Idx;
             unsigned BitWidth = cast<IntegerType>(
                     I.getOperand()->getType())->getBitWidth();
             if (BitWidth == 32)
             {
                 Idx = static_cast<int64_t>(static_cast<int32_t>(IdxGV.IntVal.getZExtValue()));
             }
             else
             {
                 assert(BitWidth == 64 && "Invalid index type for getelementptr");
                 Idx = static_cast<int64_t>(IdxGV.IntVal.getZExtValue());
             }
             Total += DL.getTypeAllocSize(I.getIndexedType()) * Idx;
         }
     }
 
     GenericValue Result;
     Result.PointerVal = static_cast<char*>(GC.getOperandValue(Ptr, SF).PointerVal) + Total;
     return Result;
 }

} // llvmir_emul
} // retdec
