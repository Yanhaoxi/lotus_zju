#include "Alias/OriginAA/Flags.h"

using namespace llvm;

// global command-line options
cl::opt<std::string> AnalysisMode(
    "pa-mode",
    cl::desc("Pointer analysis mode: 'ci' (context-insensitive) or 'kcs' (k-callsite-sensitive) or 'origin' (origin pointer analysis)"),
    cl::init("ci"));

cl::opt<unsigned> KValue(
    "k",
    cl::desc("Value of k for k-callsite-sensitive analysis"),
    cl::init(1));

cl::opt<bool> DebugMode(
    "origin-debug",
    cl::desc("Enable debug mode for OriginAA analysis"),
    cl::init(false));

cl::opt<bool> HandleIndirectCalls(
    "indirect",
    cl::desc("Enable handling of indirect function calls"),
    cl::init(false));

cl::opt<unsigned> MaxVisit(
    "max-visit",
    cl::desc("Maximum number of times a CGNode can be visited"),
    cl::init(2));

cl::opt<bool> OutputToFile(
    "output",
    cl::desc("Output pointer analysis results to a file"),
    cl::init(false));