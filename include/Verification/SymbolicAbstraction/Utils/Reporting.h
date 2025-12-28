// SymbolicAbstraction reporting helpers: configuration and analysis result rendering
#pragma once

#include <string>

namespace llvm {
class Function;
} // namespace llvm

namespace symbolic_abstraction {
class Analyzer;
} // namespace symbolic_abstraction

// Print a concise, uniform "Effective configuration" section.
void printEffectiveConfiguration(
    const std::string& configSource,
    const std::string& domainName,
    const std::string& domainSource,
    bool fallbackToFirst,
    const std::string& fragmentStrategy,
    const std::string& fragmentOrigin,
    const std::string& analyzerVariant,
    bool incremental,
    int wideningDelay,
    int wideningFrequency,
    const std::string& wideningOrigin,
    const std::string& memoryVariant,
    int addressBits,
    const std::string& memoryOrigin);

// Pretty-print analysis results at the function entry.
void printEntryResult(symbolic_abstraction::Analyzer* analyzer, llvm::Function* func);

// Pretty-print analysis results for all basic blocks.
void printAllBlocksResults(symbolic_abstraction::Analyzer* analyzer, llvm::Function* func);

// Pretty-print analysis results for exit blocks (with return).
void printExitBlocksResults(symbolic_abstraction::Analyzer* analyzer, llvm::Function* func);


