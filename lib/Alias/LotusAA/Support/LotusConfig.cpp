/*
 * LotusAA - Configuration Constants Implementation
 * 
 * Definitions for configuration constants declared in LotusConfig.h.
 * Required for linking when addresses of constants are taken.
 */

#include "Alias/LotusAA/Support/LotusConfig.h"

namespace llvm {
namespace LotusConfig {

// CallGraphLimits definitions
constexpr int CallGraphLimits::DEFAULT_MAX_ITERATIONS;
constexpr int CallGraphLimits::DEFAULT_MAX_TARGETS;
constexpr int CallGraphLimits::MAX_CALLGRAPH_SIZE;

// InterproceduralLimits definitions
constexpr int InterproceduralLimits::DEFAULT_INLINE_DEPTH;
constexpr int InterproceduralLimits::DEFAULT_MAX_INLINE_SIZE;
constexpr int InterproceduralLimits::DEFAULT_MAX_ACCESS_PATH_LEVEL;

// MemoryLimits definitions
constexpr int MemoryLimits::DEFAULT_MAX_BB_LOAD;
constexpr int MemoryLimits::DEFAULT_MAX_BB_DEPTH;
constexpr int MemoryLimits::DEFAULT_MAX_LOAD;
constexpr int MemoryLimits::DEFAULT_STORE_DEPTH;

// TimeoutLimits definitions
constexpr double TimeoutLimits::DEFAULT_PER_FUNCTION_TIMEOUT;

// Heuristics definitions
constexpr bool Heuristics::DEFAULT_ENABLE_GLOBAL_HEURISTIC;
constexpr int Heuristics::MAX_SUMMARY_AP_DEPTH;
constexpr float Heuristics::MEM_CHANGE_PROBABILITY[];
constexpr float Heuristics::COND_SAT_PROBABILITY;

// DebugOptions definitions
constexpr bool DebugOptions::DEFAULT_PRINT_PTS;
constexpr bool DebugOptions::DEFAULT_PRINT_CG;
constexpr bool DebugOptions::DEFAULT_ENABLE_CG;

} // namespace LotusConfig
} // namespace llvm
