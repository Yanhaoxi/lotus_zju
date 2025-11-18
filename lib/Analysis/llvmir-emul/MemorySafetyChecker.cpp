/**
 * @file MemorySafetyChecker.cpp
 * @brief Implementation of memory safety bug checker

 */

#include "Analysis/llvmir-emul/BugCheckers.h"
#include "Analysis/llvmir-emul/MiriEmulator.h"
#include "Analysis/llvmir-emul/llvmir_emul.h"
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>
#include <sstream>

// GVTOP and PTOGV are macros from LLVM ExecutionEngine
// They convert between GenericValue and void*
// GVTOP(gv) -> void* (from GenericValue)
// PTOGV(ptr) -> GenericValue (from void*)
// These should be available via LLVM headers

namespace miri {

void MemorySafetyChecker::preVisit(llvm::Instruction& I, MiriEmulator& emulator) {
    if (auto* LI = llvm::dyn_cast<llvm::LoadInst>(&I)) {
        checkLoad(*LI, emulator);
    } else if (auto* SI = llvm::dyn_cast<llvm::StoreInst>(&I)) {
        checkStore(*SI, emulator);
    } else if (auto* CI = llvm::dyn_cast<llvm::CallInst>(&I)) {
        llvm::Function* calledFunc = CI->getCalledFunction();
        if (calledFunc) {
            llvm::StringRef name = calledFunc->getName();
            if (name == "free") {
                checkFree(*CI, emulator);
            } else if (name == "memcpy" || name == "llvm.memcpy.p0.p0.i32" ||
                       name == "llvm.memcpy.p0.p0.i64") {
                checkMemCpy(*CI, emulator);
            } else if (name == "memset" || name == "llvm.memset.p0.i32" ||
                       name == "llvm.memset.p0.i64") {
                checkMemSet(*CI, emulator);
            }
        }
    } else if (auto* AI = llvm::dyn_cast<llvm::AllocaInst>(&I)) {
        checkAlloca(*AI, emulator);
    }
}

void MemorySafetyChecker::checkLoad(llvm::LoadInst& I, MiriEmulator& emulator) {
    // Get pointer operand
    llvm::Value* ptrOp = I.getPointerOperand();
    
    // Get execution context
    if (!emulator.hasExecutionContext()) {
        return;
    }
    
    auto& ec = emulator.getCurrentExecutionContext();
    auto& globalEc = emulator.getGlobalExecutionContext();
    llvm::GenericValue ptrVal = globalEc.getOperandValue(ptrOp, ec);
    uint64_t addr = reinterpret_cast<uint64_t>(GVTOP(ptrVal));
    
    // Get size of load
    llvm::Type* loadTy = I.getType();
    size_t size = emulator.getModule()->getDataLayout().getTypeStoreSize(loadTy);
    
    // Check the access
    checkPointerAccess(I, emulator, addr, size, false, "load");
}

void MemorySafetyChecker::checkStore(llvm::StoreInst& I, MiriEmulator& emulator) {
    // Get pointer operand
    llvm::Value* ptrOp = I.getPointerOperand();
    
    // Get execution context
    if (!emulator.hasExecutionContext()) {
        return;
    }
    
    auto& ec = emulator.getCurrentExecutionContext();
    auto& globalEc = emulator.getGlobalExecutionContext();
    llvm::GenericValue ptrVal = globalEc.getOperandValue(ptrOp, ec);
    uint64_t addr = reinterpret_cast<uint64_t>(GVTOP(ptrVal));
    
    // Get size of store
    llvm::Type* storeTy = I.getValueOperand()->getType();
    size_t size = emulator.getModule()->getDataLayout().getTypeStoreSize(storeTy);
    
    // Check the access
    checkPointerAccess(I, emulator, addr, size, true, "store");
}

void MemorySafetyChecker::checkMemCpy(llvm::CallInst& I, MiriEmulator& emulator) {
    if (!emulator.hasExecutionContext()) {
        return;
    }
    
    auto& ec = emulator.getCurrentExecutionContext();
    auto& globalEc = emulator.getGlobalExecutionContext();
    
    // Get destination and source pointers
    if (I.arg_size() < 3) {
        return;
    }
    
    llvm::GenericValue dstVal = globalEc.getOperandValue(I.getArgOperand(0), ec);
    llvm::GenericValue srcVal = globalEc.getOperandValue(I.getArgOperand(1), ec);
    llvm::GenericValue lenVal = globalEc.getOperandValue(I.getArgOperand(2), ec);
    
    uint64_t dst = reinterpret_cast<uint64_t>(GVTOP(dstVal));
    uint64_t src = reinterpret_cast<uint64_t>(GVTOP(srcVal));
    size_t len = lenVal.IntVal.getZExtValue();
    
    // Check both source and destination
    checkPointerAccess(I, emulator, dst, len, true, "memcpy destination");
    checkPointerAccess(I, emulator, src, len, false, "memcpy source");
}

void MemorySafetyChecker::checkMemSet(llvm::CallInst& I, MiriEmulator& emulator) {
    if (!emulator.hasExecutionContext()) {
        return;
    }
    
    auto& ec = emulator.getCurrentExecutionContext();
    auto& globalEc = emulator.getGlobalExecutionContext();
    
    // Get destination pointer and length
    if (I.arg_size() < 3) {
        return;
    }
    
    llvm::GenericValue dstVal = globalEc.getOperandValue(I.getArgOperand(0), ec);
    llvm::GenericValue lenVal = globalEc.getOperandValue(I.getArgOperand(2), ec);
    
    uint64_t dst = reinterpret_cast<uint64_t>(GVTOP(dstVal));
    size_t len = lenVal.IntVal.getZExtValue();
    
    // Check destination
    checkPointerAccess(I, emulator, dst, len, true, "memset");
}

void MemorySafetyChecker::checkFree(llvm::CallInst& I, MiriEmulator& emulator) {
    if (!emulator.hasExecutionContext()) {
        return;
    }
    
    auto& ec = emulator.getCurrentExecutionContext();
    auto& globalEc = emulator.getGlobalExecutionContext();
    
    // Get pointer to free
    if (I.arg_size() < 1) {
        return;
    }
    
    llvm::GenericValue ptrVal = globalEc.getOperandValue(I.getArgOperand(0), ec);
    uint64_t addr = reinterpret_cast<uint64_t>(GVTOP(ptrVal));
    
    // Check free validity
    CheckResult result = emulator.getMemoryModel().markFreed(addr, &I);
    
    if (!result.isOK()) {
        BugContext ctx;
        ctx.access_addr = addr;
        
        if (result.region) {
            ctx.region_base = result.region->getBase();
            ctx.region_size = result.region->getSize();
            ctx.alloc_site = result.region->getAllocSite();
            ctx.free_site = result.region->getFreeSite();
        }
        
        // Determine bug type based on status
        BugType type;
        switch (result.status) {
            case CheckResult::Status::DoubleFree:
                type = BugType::DoubleFree;
                break;
            case CheckResult::Status::InvalidPointer:
            case CheckResult::Status::NullPointerDeref:
                type = BugType::InvalidFree;
                break;
            default:
                type = BugType::InvalidFree;
                break;
        }
        
        emulator.reportBug(type, &I, result.message, ctx);
    }
}

void MemorySafetyChecker::checkAlloca(llvm::AllocaInst& I, MiriEmulator& emulator) {
    // Alloca checks are primarily handled by the emulator
    // We just verify the allocation succeeded
    // Additional checks could be added here for stack size limits
}

void MemorySafetyChecker::checkPointerAccess(llvm::Instruction& I,
                                              MiriEmulator& emulator,
                                              uint64_t addr, size_t size,
                                              bool is_write,
                                              const char* operation) {
    // Check the access in memory model
    CheckResult result = emulator.getMemoryModel().checkAccess(
        addr, size, is_write, false  // Don't check init here, separate checker does that
    );
    
    if (!result.isOK()) {
        // Build bug context
        BugContext ctx;
        ctx.setMemoryAccess(addr, size, is_write);
        
        if (result.region) {
            ctx.setMemoryRegion(result.region->getBase(), result.region->getSize());
            ctx.alloc_site = result.region->getAllocSite();
            ctx.free_site = result.region->getFreeSite();
        }
        
        // Build message
        std::ostringstream oss;
        oss << "Memory safety violation during " << operation << ": " << result.message;
        
        // Determine bug type
        BugType type;
        switch (result.status) {
            case CheckResult::Status::OutOfBounds:
                if (addr < result.region->getBase()) {
                    type = BugType::BufferUnderflow;
                } else {
                    type = BugType::BufferOverflow;
                }
                break;
            case CheckResult::Status::UseAfterFree:
                type = BugType::UseAfterFree;
                break;
            case CheckResult::Status::NullPointerDeref:
                type = BugType::NullPointerDeref;
                break;
            case CheckResult::Status::InvalidPointer:
                // Could be buffer overflow or invalid pointer arithmetic
                type = BugType::BufferOverflow;
                break;
            default:
                type = BugType::BufferOverflow;
                break;
        }
        
        emulator.reportBug(type, &I, oss.str(), ctx);
    }
}

} // namespace miri

