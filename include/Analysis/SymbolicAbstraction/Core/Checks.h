// SymbolicAbstraction check helpers: assertion and memory-safety reporting
#pragma once

#include <llvm/IR/Function.h>

namespace symbolic_abstraction {
class Analyzer;
}

// Runs the assertion violation check on the given function using the
// provided analyzer state. Prints results to llvm::outs().
// Returns a small non-negative number of violations; clamps large counts.
int runAssertionCheck(symbolic_abstraction::Analyzer* analyzer, llvm::Function* targetFunc);

// Runs a conservative memory-safety check for load/store pointer validity.
// Requires RTTI for dynamic_cast on abstract values. Prints results to outs().
// Returns a small non-negative number of potential violations; clamps large counts.
int runMemSafetyCheck(symbolic_abstraction::Analyzer* analyzer, llvm::Function* targetFunc);


