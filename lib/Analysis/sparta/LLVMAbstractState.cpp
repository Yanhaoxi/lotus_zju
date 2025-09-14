/*
 * LLVM Abstract State Implementation
 * State management and memory operations for LLVM abstract interpreter
 */

#include <Analysis/sparta/LLVMAbstractInterpreter.h>

#include <llvm/Support/raw_ostream.h>

namespace sparta {
namespace llvm_ai {

// ============================================================================
// LLVMAbstractState Implementation
// ============================================================================

std::ostream& operator<<(std::ostream& os, const LLVMAbstractState& state) {
    if (state.is_bottom()) {
        os << "State: ⊥";
        return os;
    }
    if (state.is_top()) {
        os << "State: ⊤";
        return os;
    }
    
    os << "Values: [environment with " << state.m_values.size() << " bindings]\n";
    os << "Memory: [memory domain]";
    return os;
}

} // namespace llvm_ai
} // namespace sparta
