/**
 * @file src/llvmir-emul/execution_context.cpp
 * @brief Execution context implementations.
 * @copyright (c) 2017 Avast Software, licensed under the MIT license
 */

#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/ConstantExpr.h>

#include "Analysis/llvmir-emul/llvmir_emul.h"

using namespace llvm;

namespace retdec {
namespace llvmir_emul {
 //
 //=============================================================================
 // GlobalExecutionContext
 //=============================================================================
 //
 
 GlobalExecutionContext::GlobalExecutionContext(llvm::Module* m) :
         _module(m)
 {
 
 }
 
 llvm::Module* GlobalExecutionContext::getModule() const
 {
     return _module;
 }
 
 llvm::GenericValue GlobalExecutionContext::getMemory(uint64_t addr, bool log)
 {
     if (log)
     {
         memoryLoads.push_back(addr);
     }
 
     auto fIt = memory.find(addr);
     return fIt != memory.end() ? fIt->second : GenericValue();
 }
 
 void GlobalExecutionContext::setMemory(
         uint64_t addr,
         llvm::GenericValue val,
         bool log)
 {
     if (log)
     {
         memoryStores.push_back(addr);
     }
 
     memory[addr] = val;
 }
 
 llvm::GenericValue GlobalExecutionContext::getGlobal(
         llvm::GlobalVariable* g,
         bool log)
 {
     if (log)
     {
         globalsLoads.push_back(g);
     }
 
     auto fIt = globals.find(g);
     assert(fIt != globals.end());
     return fIt != globals.end() ? fIt->second : GenericValue();
 }
 
 void GlobalExecutionContext::setGlobal(
         llvm::GlobalVariable* g,
         llvm::GenericValue val,
         bool log)
 {
     if (log)
     {
         globalsStores.push_back(g);
     }
 
     globals[g] = val;
 }
 
 void GlobalExecutionContext::setValue(llvm::Value* v, llvm::GenericValue val)
 {
     values[v] = val;
 }
 
 llvm::GenericValue GlobalExecutionContext::getOperandValue(
         llvm::Value* val,
         LocalExecutionContext& ec)
 {
     if (ConstantExpr* ce = dyn_cast<ConstantExpr>(val))
     {
         return getConstantExprValue(ce, ec, *this);
     }
     else if (Constant* cpv = dyn_cast<Constant>(val))
     {
         return getConstantValue(cpv, getModule());
     }
     else if (isa<GlobalValue>(val))
     {
         assert(false && "get pointer to global variable, how?");
         throw LlvmIrEmulatorError("not implemented");
     }
     else
     {
         return values[val];
     }
 }
 
 //
 //=============================================================================
 // ExecutionContext
 //=============================================================================
 //
 
LocalExecutionContext::LocalExecutionContext() :
        curInst(nullptr),
        caller(nullptr)
{

}
 
 LocalExecutionContext::LocalExecutionContext(LocalExecutionContext&& o) :
     curFunction(o.curFunction),
     curBB(o.curBB),
     curInst(o.curInst),
     caller(o.caller),
     allocas(std::move(o.allocas))
 {
 
 }
 
 LocalExecutionContext& LocalExecutionContext::operator=(LocalExecutionContext&& o)
 {
     curFunction = o.curFunction;
     curBB = o.curBB;
     curInst = o.curInst;
     caller = o.caller;
     allocas = std::move(o.allocas);
     return *this;
 }
 
 llvm::Module* LocalExecutionContext::getModule() const
 {
     return curFunction->getParent();
 }

} // llvmir_emul
} // retdec
