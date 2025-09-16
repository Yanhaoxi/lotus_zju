#pragma once
#include "llvm/Support/CommandLine.h"
#include <string>

extern llvm::cl::opt<std::string> AnalysisMode;
extern llvm::cl::opt<unsigned> KValue;
extern llvm::cl::opt<bool> DebugMode;
extern llvm::cl::opt<bool> HandleIndirectCalls;
extern llvm::cl::opt<unsigned> MaxVisit;
extern llvm::cl::opt<bool> OutputToFile;