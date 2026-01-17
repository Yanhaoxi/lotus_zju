/**
 * @file llvm-ai.cpp
 * @brief LLVM IFDS/IDE Analysis Tool
 * 
 * A command-line tool for running IFDS/IDE interprocedural dataflow analysis
 */

#include <Alias/AliasAnalysisWrapper/AliasAnalysisWrapper.h>
#include <Dataflow/IFDS/Clients/IFDSTaintAnalysis.h>
#include <Dataflow/IFDS/IFDSFramework.h>
#include <Dataflow/IFDS/IFDSSolvers.h>
#include <Utils/LLVM/Demangle.h>
#include "lotus_taint_microbench.h"

#include <llvm/ADT/Statistic.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/ErrorOr.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>

//#include <iostream>
#include <chrono>
#include <memory>
#include <sstream>
#include <string>
//#include <thread>

using namespace llvm;
using namespace ifds;

static cl::opt<std::string> InputFilename(cl::Positional, cl::desc("<input bitcode file>"),
                                          cl::Required);

static cl::opt<bool> Verbose("verbose", cl::desc("Enable verbose output"), cl::init(false));

static cl::opt<int> AnalysisType("analysis", cl::desc("Type of analysis to run: 0=taint"),
                                 cl::init(0));

static cl::opt<std::string> AliasAnalysisType("aa", 
    cl::desc("Alias analysis type: andersen, dyck, cfl-anders, cfl-steens, seadsa, allocaa, basic, combined=Andersen(NoCtx)+DyckAA (default: dyck)"),
    cl::init("dyck"));

static cl::opt<bool> ShowResults("show-results", cl::desc("Show detailed analysis results"), 
                                 cl::init(true));

static cl::opt<int> MaxDetailedResults("max-results", cl::desc("Maximum number of detailed results to show"), 
                                      cl::init(10));

static cl::opt<std::string> SourceFunctions("sources", cl::desc("Comma-separated list of source functions"),
                                            cl::init(""));

static cl::opt<std::string> SinkFunctions("sinks", cl::desc("Comma-separated list of sink functions"),
                                          cl::init(""));

static cl::opt<bool> MicroBench("micro-bench",
                                cl::desc("Enable micro benchmark mode (use source/sink and evaluate precision/recall)"),
                                cl::init(false));

static cl::opt<std::string> ExpectedFile("expected",
                                         cl::desc("Path to .expected file for micro benchmark evaluation"),
                                         cl::init(""));

static cl::opt<bool> PrintStats("print-stats", cl::desc("Print LLVM statistics"),
                                cl::init(false));

// Helper function to parse comma-separated function names
std::vector<std::string> parseFunctionList(const std::string& input) {
    std::vector<std::string> functions;
    if (input.empty()) return functions;
    
    std::stringstream ss(input);
    std::string item;
    while (std::getline(ss, item, ',')) {
        if (!item.empty()) {
            functions.push_back(item);
        }
    }
    return functions;
}

static void dumpSourceSinkMatches(const llvm::Module& module,
                                  const TaintAnalysis& analysis,
                                  llvm::raw_ostream& OS) {
    size_t total_calls = 0;
    size_t source_calls = 0;
    size_t sink_calls = 0;

    auto demangle_name = [](const std::string& name) {
        return DemangleUtils::demangle(name);
    };

    OS << "\nDetected call sites (source/sink tagging):\n";
    OS << "=========================================\n";

    for (const auto& function : module) {
        for (const auto& inst : instructions(function)) {
            const auto* call = llvm::dyn_cast<llvm::CallInst>(&inst);
            if (!call || !call->getCalledFunction()) continue;

            ++total_calls;
            bool is_source = analysis.is_source(call);
            bool is_sink = analysis.is_sink(call);
            if (is_source) ++source_calls;
            if (is_sink) ++sink_calls;

            auto raw_name = call->getCalledFunction()->getName().str();
            auto demangled_name = demangle_name(raw_name);
            auto line = call->getDebugLoc().getLine();

            OS << "  ";
            if (is_source) OS << "[source] ";
            if (is_sink) OS << "[sink] ";
            if (!is_source && !is_sink) OS << "[ ] ";
            OS << raw_name;
            if (demangled_name != raw_name) {
                OS << " -> " << demangled_name;
            }
            if (line > 0) {
                OS << " @ line " << line;
            }
            OS << "\n";
        }
    }

    OS << "Summary: " << total_calls << " calls, "
       << source_calls << " sources, "
       << sink_calls << " sinks\n";
}

// Helper function to parse alias analysis type from string
lotus::AAType parseAliasAnalysisType(const std::string& aaTypeStr) {
    std::string lowerStr = aaTypeStr;
    std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(), ::tolower);
    
    if (lowerStr == "andersen") return lotus::AAType::Andersen;
    if (lowerStr == "andersen-nocontext" || lowerStr == "andersen-noctx" ||
        lowerStr == "andersen-0cfa" || lowerStr == "andersen0" ||
        lowerStr == "nocx" || lowerStr == "noctx") return lotus::AAType::Andersen;
    if (lowerStr == "andersen-1cfa" || lowerStr == "andersen1" ||
        lowerStr == "1cfa") return lotus::AAType::Andersen1CFA;
    if (lowerStr == "andersen-2cfa" || lowerStr == "andersen2" ||
        lowerStr == "2cfa") return lotus::AAType::Andersen2CFA;
    if (lowerStr == "dyck" || lowerStr == "dyckaa") return lotus::AAType::DyckAA;
    if (lowerStr == "cfl-anders" || lowerStr == "cflanders") return lotus::AAType::CFLAnders;
    if (lowerStr == "cfl-steens" || lowerStr == "cflsteens") return lotus::AAType::CFLSteens;
    if (lowerStr == "seadsa") return lotus::AAType::SeaDsa;
    if (lowerStr == "allocaa" || lowerStr == "alloc") return lotus::AAType::AllocAA;
    if (lowerStr == "basic" || lowerStr == "basicaa") return lotus::AAType::BasicAA;
    if (lowerStr == "tbaa") return lotus::AAType::TBAA;
    if (lowerStr == "globals" || lowerStr == "globalsaa") return lotus::AAType::GlobalsAA;
    if (lowerStr == "scevaa" || lowerStr == "scev") return lotus::AAType::SCEVAA;
    if (lowerStr == "sraa") return lotus::AAType::SRAA;
    if (lowerStr == "combined") return lotus::AAType::Combined;
    if (lowerStr == "underapprox") return lotus::AAType::UnderApprox;
    
    // Default to DyckAA
    errs() << "Warning: Unknown alias analysis type '" << aaTypeStr 
           << "', defaulting to DyckAA\n";
    return lotus::AAType::DyckAA;
}

int main(int argc, char **argv) {
    InitLLVM X(argc, argv);
    
    cl::ParseCommandLineOptions(argc, argv, "LLVM IFDS/IDE Analysis Tool\n");
    
    // Enable statistics collection if requested
    if (PrintStats) {
        llvm::EnableStatistics();
    }
    
    // Set up LLVM context and source manager
    LLVMContext Context;
    SMDiagnostic Err;
    
    // Load the input module
    std::unique_ptr<Module> M = parseIRFile(InputFilename, Err, Context);
    if (!M) {
        Err.print(argv[0], errs());
        return 1;
    }
    
    if (Verbose) {
        outs() << "Loaded module: " << M->getName() << "\n";
        outs() << "Functions in module: " << M->size() << "\n";
    }
    
    // Set up alias analysis wrapper
    lotus::AAType aaType = parseAliasAnalysisType(AliasAnalysisType.getValue());
    auto aliasWrapper = std::make_unique<lotus::AliasAnalysisWrapper>(*M, aaType);
    
    if (Verbose) {
        outs() << "Using alias analysis: " << lotus::AliasAnalysisFactory::getTypeName(aaType) << "\n";
    }
    
    if (!aliasWrapper->isInitialized()) {
        errs() << "Warning: Alias analysis failed to initialize properly\n";
    }
    
    // Run the selected analysis
    try {
        switch (AnalysisType.getValue()) {
            case 0: { // Taint analysis
                outs() << "Running interprocedural taint analysis...\n";

                TaintAnalysis taintAnalysis;

                // Set up custom sources and sinks if provided
                auto sources = parseFunctionList(SourceFunctions);
                auto sinks = parseFunctionList(SinkFunctions);

                if (MicroBench) {
                    sources.push_back("source");
                    sinks.push_back("sink");
                }

                for (const auto& source : sources) {
                    taintAnalysis.add_source_function(source);
                }
                for (const auto& sink : sinks) {
                    taintAnalysis.add_sink_function(sink);
                }

                // Set up alias analysis
                taintAnalysis.set_alias_analysis(aliasWrapper.get());

                if (Verbose) {
                    dumpSourceSinkMatches(*M, taintAnalysis, outs());
                }

                auto analysisStart = std::chrono::high_resolution_clock::now();

                outs() << "Using sequential IFDS solver\n";

                ifds::IFDSSolver<ifds::TaintAnalysis> solver(taintAnalysis);

                // Enable progress bar when running in verbose mode
                if (Verbose) {
                    solver.set_show_progress(true);
                }

                solver.solve(*M);

                auto analysisEnd = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    analysisEnd - analysisStart);

                outs() << "Sequential analysis completed in " << duration.count() << " ms\n";

                if (ShowResults) {
                    taintAnalysis.report_vulnerabilities(solver, outs(), MaxDetailedResults.getValue());
                }

                if (MicroBench) {
                    runMicroBenchEvaluation(taintAnalysis, solver, ExpectedFile, Verbose, outs());
                }
                break;
            }
            default:
                errs() << "Unknown analysis type\n";
                return 1;
        }

        outs() << "Analysis completed successfully.\n";

    } catch (const std::exception &e) {
        errs() << "Error running analysis: " << e.what() << "\n";
        return 1;
    }
    
    // Statistics will be printed automatically at program exit if enabled
    
    return 0;
}
