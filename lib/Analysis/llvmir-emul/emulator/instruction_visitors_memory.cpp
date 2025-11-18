/**
 * @file src/llvmir-emul/instruction_visitors_memory.cpp
 * @brief Memory instruction visitor implementations.
 * @copyright (c) 2017 Avast Software, licensed under the MIT license
 */

#include "Analysis/llvmir-emul/llvmir_emul.h"
#include <llvm/IR/Instructions.h>
#include <llvm/IR/GetElementPtrTypeIterator.h>
#include <cstdlib>

using namespace llvm;

namespace retdec {
namespace llvmir_emul {
 void LlvmIrEmulator::visitAllocaInst(llvm::AllocaInst& I)
 {
     LocalExecutionContext& ec = _ecStack.back();
 
     Type* ty = I.getType()->getPointerElementType();
 
     unsigned elemN = _globalEc.getOperandValue(I.getOperand(0), ec).IntVal.getZExtValue();
     unsigned tySz = static_cast<size_t>(_module->getDataLayout().getTypeAllocSize(ty));
 
     // Avoid malloc-ing zero bytes, use max()...
     unsigned memToAlloc = std::max(1U, elemN * tySz);
 
     // Allocate enough memory to hold the type...
     void *mem = malloc(memToAlloc);
 
     GenericValue res = PTOGV(mem);
     assert(res.PointerVal && "Null pointer returned by malloc!");
     _globalEc.setValue(&I, res);
 
     if (I.getOpcode() == Instruction::Alloca)
     {
         _ecStack.back().allocas.add(mem);
     }
 }
 
 void LlvmIrEmulator::visitGetElementPtrInst(llvm::GetElementPtrInst& I)
 {
       LocalExecutionContext& ec = _ecStack.back();
       _globalEc.setValue(
               &I,
               executeGEPOperation(
                       I.getPointerOperand(),
                       gep_type_begin(I),
                       gep_type_end(I),
                       ec,
                       _globalEc));
 }
 
 void LlvmIrEmulator::visitLoadInst(llvm::LoadInst& I)
 {
     LocalExecutionContext& ec = _ecStack.back();
     GenericValue res;
 
     if (auto* gv = dyn_cast<GlobalVariable>(I.getPointerOperand()))
     {
         res = _globalEc.getGlobal(gv);
     }
     else
     {
         GenericValue src = _globalEc.getOperandValue(I.getPointerOperand(), ec);
         GenericValue* ptr = reinterpret_cast<GenericValue*>(GVTOP(src));
         uint64_t ptrVal = reinterpret_cast<uint64_t>(ptr);
         res = _globalEc.getMemory(ptrVal);
     }
 
     _globalEc.setValue(&I, res);
 }
 
 void LlvmIrEmulator::visitStoreInst(llvm::StoreInst& I)
 {
     LocalExecutionContext& ec = _ecStack.back();
     GenericValue val = _globalEc.getOperandValue(I.getOperand(0), ec);
 
     if (auto* gv = dyn_cast<GlobalVariable>(I.getPointerOperand()))
     {
         _globalEc.setGlobal(gv, val);
     }
     else
     {
         GenericValue dst = _globalEc.getOperandValue(I.getPointerOperand(), ec);
         GenericValue* ptr = reinterpret_cast<GenericValue*>(GVTOP(dst));
         uint64_t ptrVal = reinterpret_cast<uint64_t>(ptr);
         _globalEc.setMemory(ptrVal, val);
     }
 }

} // llvmir_emul
} // retdec
