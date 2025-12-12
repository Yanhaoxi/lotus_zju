/*
 * Andersen's Pointer Analysis Driver
 * 
 * This file implements the main driver for running Andersen's pointer analysis
 * on LLVM bitcode files. It parses command-line options, loads the input module,
 * runs the Andersen analysis, and outputs the results.
 *
 * Andersen's analysis is a subset-based, flow-insensitive, field-sensitive
 * pointer analysis algorithm. It supports both context-insensitive and
 * context-sensitive variants (1-CFA and 2-CFA).
 */

#include "Alias/SparrowAA/Andersen.h"
#include "Alias/SparrowAA/AndersenAA.h"
#include "Alias/SparrowAA/Log.h"

#include <llvm/Analysis/MemoryLocation.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Signals.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/ADT/Statistic.h>

#include <memory>
#include <cstring>

using namespace llvm;

enum class LogLevel { TRACE, DEBUG, INFO, WARN, ERR, OFF };

// Define option category for sparrow-aa tool options
static cl::OptionCategory SparrowAACategory("Sparrow-AA Options",
                                            "Options for the Sparrow-AA pointer analysis tool");

// Andersen analysis options (defined in lib/Alias/SparrowAA/Andersen.cpp)
extern cl::OptionCategory AndersenCategory;
extern cl::opt<unsigned> AndersenKContext;
extern cl::opt<bool> AndersenUseBDDPointsTo;

static cl::opt<std::string> InputFilename(cl::Positional, cl::desc("<input bitcode file>"),
                                          cl::Required, cl::value_desc("filename"),
                                          cl::cat(SparrowAACategory));
static cl::opt<bool> PrintPointsTo("print-pts", cl::desc("Print points-to information for all pointers"),
                                   cl::init(false), cl::cat(SparrowAACategory));
static cl::opt<bool> PrintGlobalsOnly("print-globals-only", cl::desc("Print points-to information for global variables only"),
                                      cl::init(false), cl::cat(SparrowAACategory));
static cl::opt<bool> PrintAllocSites("print-alloc-sites", cl::desc("Print all allocation sites identified"),
                                     cl::init(false), cl::cat(SparrowAACategory));
static cl::opt<bool> PrintAliasQueries("print-alias-queries", cl::desc("Perform and print alias queries between pointers"),
                                       cl::init(false), cl::cat(SparrowAACategory));
static cl::opt<bool> Verbose("v", cl::desc("Verbose output"), cl::init(false), cl::cat(SparrowAACategory));
static cl::opt<bool> OnlyStatistics("s", cl::desc("Only output statistics"), cl::init(false), cl::cat(SparrowAACategory));
static cl::opt<bool> VerifyInput("verify", cl::desc("Verify input module before analysis"), cl::init(true), cl::cat(SparrowAACategory));

static cl::opt<LogLevel> LogLevelOpt("log-level", cl::desc("Set the logging level"),
                                     cl::values(clEnumValN(LogLevel::TRACE, "trace", "Display all messages including trace information"),
                                                clEnumValN(LogLevel::DEBUG, "debug", "Display all messages including debug information"),
                                                clEnumValN(LogLevel::INFO, "info", "Display informational messages and above (default)"),
                                                clEnumValN(LogLevel::WARN, "warn", "Display warnings and errors only"),
                                                clEnumValN(LogLevel::ERR, "error", "Display errors only"),
                                                clEnumValN(LogLevel::OFF, "off", "Suppress all log output")),
                                     cl::init(LogLevel::INFO), cl::cat(SparrowAACategory));
static cl::opt<bool> QuietLogging("quiet", cl::desc("Suppress most log output (equivalent to --log-level=off)"),
                                  cl::init(false), cl::cat(SparrowAACategory));

static void printValue(const Value *V, raw_ostream &OS) {
    if (V->hasName()) OS << V->getName();
    else V->printAsOperand(OS, false);
}

static void printPointsToSet(const Value *V, Andersen &Anders, raw_ostream &OS) {
    if (!V->getType()->isPointerTy()) return;
    
    std::vector<const Value *> ptsSet;
    if (!Anders.getPointsToSet(V, ptsSet)) { OS << "  "; printValue(V, OS); OS << " points to unknown\n"; return; }
    if (ptsSet.empty()) { OS << "  "; printValue(V, OS); OS << " points to nothing\n"; return; }
    
    OS << "  "; printValue(V, OS); OS << " points to " << ptsSet.size() << " location(s):\n";
    for (const Value *T : ptsSet) {
        OS << "    - "; printValue(T, OS);
        if (isa<GlobalVariable>(T)) OS << " [global]";
        else if (isa<AllocaInst>(T)) OS << " [stack]";
        else if (isa<CallInst>(T) || isa<InvokeInst>(T)) OS << " [heap]";
        else if (isa<Function>(T)) OS << " [function]";
        OS << "\n";
    }
}

static void performAliasQueries(Module &M, AndersenAAResult &AAResult, raw_ostream &OS) {
    OS << "\n=== Alias Query Results ===\n\n";
    
    std::vector<const Value *> Pointers;
    for (const GlobalVariable &GV : M.globals())
        if (GV.getType()->isPointerTy()) Pointers.push_back(&GV);
    for (const Function &F : M) {
        if (F.isDeclaration()) continue;
        for (const BasicBlock &BB : F)
            for (const Instruction &I : BB)
                if (I.getType()->isPointerTy()) Pointers.push_back(&I);
    }
    
    OS << "Total pointers: " << Pointers.size() << "\n\n";
    
    unsigned Counts[3] = {0};
    const char *Names[] = {"NoAlias", "MayAlias", "MustAlias"};
    
    for (size_t i = 0; i < Pointers.size() && i < 20; ++i) {
        for (size_t j = i + 1; j < Pointers.size() && j < 20; ++j) {
            AliasResult R = AAResult.alias(MemoryLocation(Pointers[i], LocationSize::beforeOrAfterPointer()),
                                          MemoryLocation(Pointers[j], LocationSize::beforeOrAfterPointer()));
            int idx = (R == AliasResult::MayAlias) ? 1 : (R == AliasResult::MustAlias) ? 2 : 0;
            Counts[idx]++;
            
            if (R != AliasResult::NoAlias) {
                OS << "  "; Pointers[i]->printAsOperand(OS, false);
                OS << " and "; Pointers[j]->printAsOperand(OS, false);
                OS << " -> " << Names[idx] << "\n";
            }
        }
    }
    
    OS << "\n--- Summary ---\n";
    for (int i = 0; i < 3; ++i) OS << Names[i] << ": " << Counts[i] << "\n";
}

int main(int argc, char **argv) {
    InitLLVM X(argc, argv);
    volatile unsigned dummy_k = AndersenKContext;
    volatile bool dummy_bdd = AndersenUseBDDPointsTo;
    (void)dummy_k; (void)dummy_bdd;
    
    cl::HideUnrelatedOptions({&SparrowAACategory, &AndersenCategory});
    cl::ParseCommandLineOptions(argc, argv, 
        "Andersen's Pointer Analysis Tool\n\n"
        "Subset-based, flow-insensitive, field-sensitive pointer analysis.\n\n"
        "Context Sensitivity:\n"
        "  --andersen-k-cs=<0|1|2>  Select call-site sensitivity:\n"
        "                            0 = context-insensitive (default)\n"
        "                            1 = 1-CFA\n"
        "                            2 = 2-CFA\n");

    selectGlobalPtsSetImpl(AndersenUseBDDPointsTo ? PtsSetImpl::BDD : PtsSetImpl::SPARSE_BITVECTOR);

    LogLevel effectiveLevel = QuietLogging ? LogLevel::OFF : LogLevelOpt;
    static const spdlog::level::level_enum levels[] = {
        spdlog::level::trace, spdlog::level::debug, spdlog::level::info,
        spdlog::level::warn, spdlog::level::err, spdlog::level::off
    };
    spdlog::set_level(levels[static_cast<int>(effectiveLevel)]);
    spdlog::set_pattern("%^[%l]%$ %v");

    LLVMContext Context;
    SMDiagnostic Err;

    if (Verbose && !OnlyStatistics) errs() << "Loading: " << InputFilename << "\n";
    
    std::unique_ptr<Module> M = parseIRFile(InputFilename, Err, Context);
    if (!M) { Err.print(argv[0], errs()); return 1; }
    if (VerifyInput && verifyModule(*M, &errs())) {
        errs() << "Module verification failed\n";
        return 1;
    }

    ContextPolicy policy = getSelectedAndersenContextPolicy();
    if (Verbose && !OnlyStatistics) {
        errs() << "Module: " << M->getName() << " (" << M->getFunctionList().size()
               << " functions, " << M->getGlobalList().size() << " globals)\n"
               << "Context sensitivity: " << policy.name << "\nRunning analysis...\n";
    }

    Andersen Anders(*M, policy);
    if (Verbose && !OnlyStatistics) errs() << "Done.\n\n";

    if (!OnlyStatistics) {
        outs() << "\n=== Andersen Analysis Results ===";
        if (policy.name && strcmp(policy.name, "NoCtx") != 0) outs() << " (" << policy.name << ")";
        outs() << "\n\n";
        
        if (PrintAllocSites) {
            std::vector<const Value *> allocSites;
            Anders.getAllAllocationSites(allocSites);
            outs() << "--- Allocation Sites (" << allocSites.size() << ") ---\n\n";
            for (const Value *V : allocSites) {
                outs() << "  "; printValue(V, outs());
                if (auto *GV = dyn_cast<GlobalVariable>(V))
                    outs() << " [global, " << (GV->isConstant() ? "const]" : "mutable]");
                else if (auto *AI = dyn_cast<AllocaInst>(V))
                    outs() << " [stack, in " << AI->getFunction()->getName() << "]";
                else if (isa<CallInst>(V)) outs() << " [heap]";
                else if (isa<Function>(V)) outs() << " [function]";
                outs() << "\n";
            }
            outs() << "\n";
        }
        
        if (PrintPointsTo || PrintGlobalsOnly) {
            outs() << "--- Points-To Information ---\n\nGlobal Variables:\n";
            bool found = false;
            for (const GlobalVariable &GV : M->globals())
                if (GV.getType()->isPointerTy()) { found = true; printPointsToSet(&GV, Anders, outs()); }
            if (!found) outs() << "  (none)\n";
            outs() << "\n";
            
            if (PrintPointsTo && !PrintGlobalsOnly) {
                for (const Function &F : *M) {
                    if (F.isDeclaration()) continue;
                    bool header = false;
                    for (const Argument &A : F.args())
                        if (A.getType()->isPointerTy()) {
                            if (!header) { outs() << "Function: " << F.getName() << "\n"; header = true; }
                            outs() << "  Arg: "; printPointsToSet(&A, Anders, outs());
                        }
                    for (const BasicBlock &BB : F)
                        for (const Instruction &I : BB)
                            if (I.getType()->isPointerTy()) {
                                if (!header) { outs() << "Function: " << F.getName() << "\n"; header = true; }
                                printPointsToSet(&I, Anders, outs());
                            }
                    if (header) outs() << "\n";
                }
            }
        }
        
        if (PrintAliasQueries) {
            AndersenAAResult AAResult(*M);
            performAliasQueries(*M, AAResult, outs());
        }
        outs() << "\nAnalysis completed.\n";
    }

    if (OnlyStatistics || Verbose) {
        errs() << "\n=== Statistics ===\n";
        PrintStatistics(errs());
    }

    return 0;
}

