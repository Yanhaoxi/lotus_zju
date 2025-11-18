/**
 * @file UninitializedMemoryChecker.cpp
 * @brief Implementation of uninitialized memory checker

 */

#include "Analysis/llvmir-emul/BugCheckers.h"
#include "Analysis/llvmir-emul/MiriEmulator.h"
#include "Analysis/llvmir-emul/llvmir_emul.h"
#include <llvm/IR/Instructions.h>
#include <sstream>

// GVTOP is a macro from LLVM ExecutionEngine
// It converts GenericValue to void*

namespace miri {

void UninitializedMemoryChecker::preVisit(llvm::Instruction& I, MiriEmulator& emulator) {
    if (auto* LI = llvm::dyn_cast<llvm::LoadInst>(&I)) {
        checkLoad(*LI, emulator);
    }
}

void UninitializedMemoryChecker::checkLoad(llvm::LoadInst& I, MiriEmulator& emulator) {
    if (!emulator.hasExecutionContext()) {
        return;
    }
    
    auto& ec = emulator.getCurrentExecutionContext();
    auto& globalEc = emulator.getGlobalExecutionContext();
    
    // Get pointer operand
    llvm::Value* ptrOp = I.getPointerOperand();
    llvm::GenericValue ptrVal = globalEc.getOperandValue(ptrOp, ec);
    uint64_t addr = reinterpret_cast<uint64_t>(GVTOP(ptrVal));
    
    // Get size of load
    llvm::Type* loadTy = I.getType();
    size_t size = emulator.getModule()->getDataLayout().getTypeStoreSize(loadTy);
    
    // Check if memory is initialized
    CheckResult result = emulator.getMemoryModel().checkAccess(
        addr, size, false, true  // is_write=false, check_init=true
    );
    
    if (result.status == CheckResult::Status::UninitializedRead) {
        BugContext ctx;
        ctx.setMemoryAccess(addr, size, false);
        
        if (result.region) {
            ctx.setMemoryRegion(result.region->getBase(), result.region->getSize());
            ctx.alloc_site = result.region->getAllocSite();
        }
        
        std::ostringstream oss;
        oss << "Reading uninitialized memory: " << result.message;
        
        emulator.reportBug(BugType::UninitializedRead, &I, oss.str(), ctx);
    }
}

} // namespace miri

