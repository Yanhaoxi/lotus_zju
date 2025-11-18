/**
 * @file NullPointerChecker.cpp
 * @brief Implementation of null pointer dereference checker

 */

#include "Analysis/llvmir-emul/BugCheckers.h"
#include "Analysis/llvmir-emul/MiriEmulator.h"
#include "Analysis/llvmir-emul/llvmir_emul.h"
#include <llvm/IR/Instructions.h>
#include <sstream>

// GVTOP is a macro from LLVM ExecutionEngine
// It converts GenericValue to void*

namespace miri {

void NullPointerChecker::preVisit(llvm::Instruction& I, MiriEmulator& emulator) {
    if (llvm::isa<llvm::LoadInst>(&I) || llvm::isa<llvm::StoreInst>(&I)) {
        checkLoadStore(I, emulator);
    } else if (auto* CI = llvm::dyn_cast<llvm::CallInst>(&I)) {
        checkCall(*CI, emulator);
    }
}

void NullPointerChecker::checkLoadStore(llvm::Instruction& I, MiriEmulator& emulator) {
    if (!emulator.hasExecutionContext()) {
        return;
    }
    
    auto& ec = emulator.getCurrentExecutionContext();
    auto& globalEc = emulator.getGlobalExecutionContext();
    
    // Get pointer operand
    llvm::Value* ptrOp = nullptr;
    size_t access_size = 0;
    bool is_write = false;
    
    if (auto* LI = llvm::dyn_cast<llvm::LoadInst>(&I)) {
        ptrOp = LI->getPointerOperand();
        access_size = emulator.getModule()->getDataLayout().getTypeStoreSize(LI->getType());
        is_write = false;
    } else if (auto* SI = llvm::dyn_cast<llvm::StoreInst>(&I)) {
        ptrOp = SI->getPointerOperand();
        access_size = emulator.getModule()->getDataLayout().getTypeStoreSize(
            SI->getValueOperand()->getType());
        is_write = true;
    }
    
    if (!ptrOp) {
        return;
    }
    
    // Get pointer value
    llvm::GenericValue ptrVal = globalEc.getOperandValue(ptrOp, ec);
    uint64_t addr = reinterpret_cast<uint64_t>(GVTOP(ptrVal));
    
    // Check if pointer is null
    if (isNullPointer(addr)) {
        BugContext ctx;
        ctx.setMemoryAccess(addr, access_size, is_write);
        ctx.addValue("pointer", addr);
        
        std::ostringstream oss;
        oss << "Null pointer dereference in " 
            << (is_write ? "store" : "load") << " operation";
        
        emulator.reportBug(BugType::NullPointerDeref, &I, oss.str(), ctx);
    }
}

void NullPointerChecker::checkCall(llvm::CallInst& I, MiriEmulator& emulator) {
    if (!emulator.hasExecutionContext()) {
        return;
    }
    
    auto& ec = emulator.getCurrentExecutionContext();
    auto& globalEc = emulator.getGlobalExecutionContext();
    
    // Check if calling through a null function pointer
    llvm::Value* calledValue = I.getCalledOperand();
    
    if (calledValue && !llvm::isa<llvm::Function>(calledValue)) {
        // Indirect call through function pointer
        llvm::GenericValue ptrVal = globalEc.getOperandValue(calledValue, ec);
        uint64_t addr = reinterpret_cast<uint64_t>(GVTOP(ptrVal));
        
        if (isNullPointer(addr)) {
            BugContext ctx;
            ctx.addValue("function_pointer", addr);
            
            emulator.reportBug(BugType::NullPointerDeref, &I,
                             "Call through null function pointer", ctx);
        }
    }
    
    // Also check pointer arguments to functions like memcpy
    llvm::Function* calledFunc = I.getCalledFunction();
    if (calledFunc) {
        llvm::StringRef name = calledFunc->getName();
        
        // Check common memory functions
        if (name == "memcpy" || name == "memmove" || name == "memset" ||
            name.startswith("llvm.memcpy") || name.startswith("llvm.memmove") ||
            name.startswith("llvm.memset")) {
            
            // Check destination pointer (first argument)
            if (I.arg_size() >= 1) {
                llvm::GenericValue dstVal = globalEc.getOperandValue(
                    I.getArgOperand(0), ec);
                uint64_t dst = reinterpret_cast<uint64_t>(GVTOP(dstVal));
                
                if (isNullPointer(dst)) {
                    BugContext ctx;
                    ctx.addValue("destination_pointer", dst);
                    
                    std::ostringstream oss;
                    oss << "Null pointer passed as destination to " << name.str();
                    
                    emulator.reportBug(BugType::NullPointerDeref, &I, oss.str(), ctx);
                }
            }
            
            // Check source pointer for memcpy/memmove
            if ((name == "memcpy" || name == "memmove" ||
                 name.startswith("llvm.memcpy") || name.startswith("llvm.memmove")) &&
                I.arg_size() >= 2) {
                
                llvm::GenericValue srcVal = globalEc.getOperandValue(
                    I.getArgOperand(1), ec);
                uint64_t src = reinterpret_cast<uint64_t>(GVTOP(srcVal));
                
                if (isNullPointer(src)) {
                    BugContext ctx;
                    ctx.addValue("source_pointer", src);
                    
                    std::ostringstream oss;
                    oss << "Null pointer passed as source to " << name.str();
                    
                    emulator.reportBug(BugType::NullPointerDeref, &I, oss.str(), ctx);
                }
            }
        }
    }
}

bool NullPointerChecker::isNullPointer(uint64_t ptr) {
    return ptr < null_threshold_;
}

} // namespace miri

