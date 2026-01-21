/**
 * @file Program.cpp
 * @brief Program representation and call site resolution for AserPTA.
 *
 * Provides utilities for working with LLVM programs in the context of pointer
 * analysis, including call site target resolution and indirect call handling.
 *
 * @author peiming
 */
#include <llvm/Support/CommandLine.h>
#include <set>

#include "Alias/AserPTA/PointerAnalysis/Program/CallSite.h"
#include "Alias/AserPTA/Util/Log.h"

using namespace aser;
using namespace llvm;

llvm::cl::opt<size_t> MaxIndirectTarget("max-indirect-target",
                                  llvm::cl::init(std::numeric_limits<size_t>::max()),  // by default no limitation
                                  llvm::cl::desc("max number of indirect call target that can be resolved by indirect call"));

/**
 * @brief Resolve the target function from a called value.
 *
 * Attempts to resolve the target function for direct calls by handling
 * bitcasts and global aliases. For indirect calls, this will fail and
 * trigger an assertion.
 *
 * @param calledValue The value being called (function pointer or function)
 * @return The resolved Function pointer, or nullptr if resolution fails
 */
const Function* aser::CallSite::resolveTargetFunction(const Value* calledValue) {
    // TODO: In this case, a constant expression/global aliases, which can be
    // resolved directly
    if (auto* bitcast = dyn_cast<BitCastOperator>(calledValue)) {
        if (auto* function = dyn_cast<Function>(bitcast->getOperand(0))) {
            return function;
        }
    }

    if (auto* globalAlias = dyn_cast<GlobalAlias>(calledValue)) {
        auto* globalSymbol = globalAlias->getAliasee()->stripPointerCasts();
        if (auto* function = dyn_cast<Function>(globalSymbol)) {
            return function;
        }
        LOG_ERROR("Unhandled Global Alias. alias={}", *globalAlias);
        llvm_unreachable(
            "resolveTargetFunction matched globalAlias but symbol was not "
            "Function");
    }

    LOG_ERROR("Unable to resolveTargetFunction from calledValue. called={}", *calledValue);
    //return nullptr;
    llvm_unreachable("Unable to resolveTargetFunction from calledValue");
}
