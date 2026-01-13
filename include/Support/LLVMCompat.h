#pragma once

#include "Support/LLVMConfig.h"
#include "Support/CallSiteCompat.h"
#include "Support/IRBuilderCompat.h"
#include "Support/OpaquePointer.h"
#include "Support/TypeCompat.h"

#include <llvm/Config/llvm-config.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>

namespace lotus {
namespace compat {

// LLVM 11+ removed CallSite and replaced it with CallBase
// We assume LLVM 12+ for this compatibility layer.

#if LLVM_VERSION_MAJOR >= 12
    // Alias CallSite to CallBase for easier porting, 
    // though ideally code should be updated to use CallBase directly.
    // Note: CallBase is not a direct replacement for CallSite wrapper, 
    // but a base class for CallInst and InvokeInst.
    using CallSite = llvm::CallBase;
#endif

// Helper to get the called function, handling cases where it might be indirect
inline llvm::Function *getCalledFunction(const llvm::CallBase *CB) {
    return CB->getCalledFunction();
}

} // namespace compat
} // namespace lotus
