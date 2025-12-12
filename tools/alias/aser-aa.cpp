/*
 * AserPTA: Pointer Analysis Tool
 * 
 * A high-performance pointer analysis tool supporting multiple context sensitivities
 * and solver algorithms.
 */

#include <llvm/ADT/Statistic.h>
#include <llvm/IR/IRPrintingPasses.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Signals.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Transforms/IPO/AlwaysInliner.h>

#include "Alias/AserPTA/PointerAnalysis/Context/KCallSite.h"
#include "Alias/AserPTA/PointerAnalysis/Context/KOrigin.h"
#include "Alias/AserPTA/PointerAnalysis/Context/NoCtx.h"
#include "Alias/AserPTA/PointerAnalysis/Models/LanguageModel/DefaultLangModel/DefaultLangModel.h"
#include "Alias/AserPTA/PointerAnalysis/Models/MemoryModel/FieldSensitive/FSMemModel.h"
#include "Alias/AserPTA/PointerAnalysis/Models/MemoryModel/FieldInsensitive/FIMemModel.h"
#include "Alias/AserPTA/PointerAnalysis/Solver/PartialUpdateSolver.h"
#include "Alias/AserPTA/PointerAnalysis/Solver/WavePropagation.h"
#include "Alias/AserPTA/PointerAnalysis/Solver/DeepPropagation.h"
#include "Alias/AserPTA/PTADriver.h"
#include "Alias/Common/AliasSpecManager.h"

using namespace aser;
using namespace llvm;
using namespace std;
using namespace lotus::alias;

// Command-line options
static cl::opt<std::string> InputFilename(cl::Positional, 
    cl::desc("<input bitcode file>"), cl::Required);

static cl::opt<std::string> AnalysisMode("analysis-mode",
    cl::desc("Analysis mode: ci (context-insensitive), 1-cfa, 2-cfa, origin"),
    cl::init("ci"), cl::value_desc("mode"));

static cl::opt<std::string> SolverType("solver",
    cl::desc("Solver type: basic, wave, deep"),
    cl::init("wave"), cl::value_desc("solver"));

static cl::opt<bool> FieldSensitive("field-sensitive",
    cl::desc("Use field-sensitive memory model"),
    cl::init(true));

static cl::opt<bool> DumpStats("dump-stats",
    cl::desc("Print analysis statistics"),
    cl::init(true));

static cl::opt<std::string> OutputFile("o",
    cl::desc("Output file for results"),
    cl::value_desc("filename"));

static cl::opt<std::string> ConfigFile("config",
    cl::desc("Path to config spec file (e.g., ptr.spec). Can be specified multiple times or use comma-separated paths"),
    cl::value_desc("filepath"));

static cl::list<std::string> ConfigFiles("config-file",
    cl::desc("Path to config spec file (alternative to -config)"),
    cl::value_desc("filepath"));

// Type aliases for analysis configurations
using Origin = KOrigin<1>;

template <typename ctx>
using FSModel = DefaultLangModel<ctx, FSMemModel<ctx>>;

template <typename ctx>
using FIModel = DefaultLangModel<ctx, FIMemModel<ctx>>;

// Solver type definitions
using CIWaveSolver = WavePropagation<FSModel<NoCtx>>;
using CIDeepSolver = DeepPropagation<FSModel<NoCtx>>;
using CIBasicSolver = PartialUpdateSolver<FSModel<NoCtx>>;

using CS1WaveSolver = WavePropagation<FSModel<KCallSite<1>>>;
using CS1DeepSolver = DeepPropagation<FSModel<KCallSite<1>>>;
using CS1BasicSolver = PartialUpdateSolver<FSModel<KCallSite<1>>>;

using CS2WaveSolver = WavePropagation<FSModel<KCallSite<2>>>;
using CS2DeepSolver = DeepPropagation<FSModel<KCallSite<2>>>;
using CS2BasicSolver = PartialUpdateSolver<FSModel<KCallSite<2>>>;

using OriginWaveSolver = WavePropagation<FSModel<Origin>>;
using OriginDeepSolver = DeepPropagation<FSModel<Origin>>;
using OriginBasicSolver = PartialUpdateSolver<FSModel<Origin>>;


int main(int argc, char** argv) {
    // Parse command line
    cl::ParseCommandLineOptions(argc, argv, 
        "AserPTA - High-Performance Pointer Analysis Tool\n");

    // Load IR module
    SMDiagnostic Err;
    LLVMContext Context;
    auto module = parseIRFile(InputFilename, Err, Context);

    if (!module) {
        Err.print(argv[0], errs());
        return 1;
    }

    errs() << "Loaded module: " << InputFilename << "\n";
    errs() << "Analysis mode: " << AnalysisMode << "\n";
    errs() << "Solver type: " << SolverType << "\n";
    errs() << "Field-sensitive: " << (FieldSensitive ? "yes" : "no") << "\n";
    
    // Initialize AliasSpecManager with config files
    std::unique_ptr<AliasSpecManager> specManager;
    std::vector<std::string> specFilePaths;
    
    // Collect config files from command-line options
    if (!ConfigFile.empty()) {
        // Parse comma-separated paths if provided
        std::string config = ConfigFile;
        size_t pos = 0;
        while ((pos = config.find(',')) != std::string::npos) {
            std::string path = config.substr(0, pos);
            if (!path.empty()) {
                specFilePaths.push_back(path);
            }
            config.erase(0, pos + 1);
        }
        if (!config.empty()) {
            specFilePaths.push_back(config);
        }
    }
    
    // Add files from -config-file option
    for (const auto &path : ConfigFiles) {
        specFilePaths.push_back(path);
    }
    
    // Create spec manager with specified files or use defaults
    if (!specFilePaths.empty()) {
        specManager = std::make_unique<AliasSpecManager>(specFilePaths);
    } else {
        specManager = std::make_unique<AliasSpecManager>();
    }
    
    // Initialize with module for better name matching
    specManager->initialize(*module);
    
    // Display loaded config files
    const auto &loadedFiles = specManager->getLoadedSpecFiles();
    if (!loadedFiles.empty()) {
        errs() << "Config files: ";
        for (size_t i = 0; i < loadedFiles.size(); ++i) {
            if (i > 0) errs() << ", ";
            errs() << loadedFiles[i];
        }
        errs() << "\n";
    } else {
        errs() << "Config files: (none loaded)\n";
    }

    // Setup origin rules for origin-sensitive analysis
    Origin::setOriginRules([](const Origin *, const llvm::Instruction *I) -> bool {
        if (auto *CB = llvm::dyn_cast<CallBase>(I)) {
            if (auto *F = CB->getCalledFunction()) {
                StringRef name = F->getName();
                // Track thread creation and spawn operations as origins
                return name.equals("pthread_create") || 
                       name.contains("spawn") ||
                       name.contains("thread");
            }
        }
        return false;
    });

    // Run analysis based on mode and solver
    try {
        if (AnalysisMode == "ci") {
            // Context-insensitive
            if (SolverType == "basic") {
                runAnalysis<CIBasicSolver>(*module, DumpStats);
            } else if (SolverType == "wave") {
                runAnalysis<CIWaveSolver>(*module, DumpStats);
            } else if (SolverType == "deep") {
                runAnalysis<CIDeepSolver>(*module, DumpStats);
            } else {
                errs() << "Unknown solver type: " << SolverType << "\n";
                return 1;
            }
        } else if (AnalysisMode == "1-cfa") {
            // 1-call-site sensitive
            if (SolverType == "basic") {
                runAnalysis<CS1BasicSolver>(*module, DumpStats);
            } else if (SolverType == "wave") {
                runAnalysis<CS1WaveSolver>(*module, DumpStats);
            } else if (SolverType == "deep") {
                runAnalysis<CS1DeepSolver>(*module, DumpStats);
            } else {
                errs() << "Unknown solver type: " << SolverType << "\n";
                return 1;
            }
        } else if (AnalysisMode == "2-cfa") {
            // 2-call-site sensitive
            if (SolverType == "basic") {
                runAnalysis<CS2BasicSolver>(*module, DumpStats);
            } else if (SolverType == "wave") {
                runAnalysis<CS2WaveSolver>(*module, DumpStats);
            } else if (SolverType == "deep") {
                runAnalysis<CS2DeepSolver>(*module, DumpStats);
            } else {
                errs() << "Unknown solver type: " << SolverType << "\n";
                return 1;
            }
        } else if (AnalysisMode == "origin") {
            // Origin-sensitive
            if (SolverType == "basic") {
                runAnalysis<OriginBasicSolver>(*module, DumpStats);
            } else if (SolverType == "wave") {
                runAnalysis<OriginWaveSolver>(*module, DumpStats);
            } else if (SolverType == "deep") {
                runAnalysis<OriginDeepSolver>(*module, DumpStats);
            } else {
                errs() << "Unknown solver type: " << SolverType << "\n";
                return 1;
            }
        } else {
            errs() << "Unknown analysis mode: " << AnalysisMode << "\n";
            errs() << "Valid modes: ci, 1-cfa, 2-cfa, origin\n";
            return 1;
        }
    } catch (const std::exception &e) {
        errs() << "Error during analysis: " << e.what() << "\n";
        return 1;
    }

    return 0;
}

