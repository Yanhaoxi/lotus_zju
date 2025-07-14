#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/ManagedStatic.h>
#include <llvm/Support/PrettyStackTrace.h>
#include <llvm/Support/Signals.h>
#include <llvm/IR/LegacyPassManager.h>

#include <string>
#include <memory>

#include "Dataflow/GVFA/GlobalValueFlowAnalysis.h"
#include "Alias/DyckAA/DyckAliasAnalysis.h"
#include "Alias/DyckAA/DyckModRefAnalysis.h"
#include "Alias/DyckAA/DyckValueFlowAnalysis.h"

using namespace llvm;

static cl::opt<std::string> InputFilename(cl::Positional,
                                         cl::desc("<input bitcode file>"),
                                         cl::Required);

static cl::opt<std::string> VulnType("vuln-type",
                                     cl::desc("Vulnerability type to check"),
                                     cl::init("nullpointer"));

static cl::opt<bool> EnableOptimized("enable-optimized",
                                    cl::desc("Use optimized analysis"),
                                    cl::init(true));

static cl::opt<bool> DumpStats("dump-stats",
                              cl::desc("Dump analysis statistics"),
                              cl::init(false));

int main(int argc, char **argv) {
    // Initialize LLVM
    sys::PrintStackTraceOnErrorSignal(argv[0]);
    PrettyStackTraceProgram X(argc, argv);
    llvm_shutdown_obj Y;
    
    // Parse command line options
    cl::ParseCommandLineOptions(argc, argv, "Dyck Global Value Flow Analysis Tool\n");
    
    // Load the module
    LLVMContext Context;
    SMDiagnostic Err;
    std::unique_ptr<Module> M = parseIRFile(InputFilename, Err, Context);
    
    if (!M) {
        Err.print(argv[0], errs());
        return 1;
    }
    
    outs() << "Loaded module: " << M->getModuleIdentifier() << "\n";
    outs() << "Functions: " << M->getFunctionList().size() << "\n";
    
    // Create PassManager and run analysis passes
    legacy::PassManager PM;
    
    // Add DyckAliasAnalysis first
    outs() << "Running Dyck Alias Analysis...\n";
    auto *DyckAAPass = new DyckAliasAnalysis();
    PM.add(DyckAAPass);
    
    // Add DyckModRefAnalysis (depends on DyckAliasAnalysis)
    outs() << "Running Dyck ModRef Analysis...\n";
    auto *DyckMRAPass = new DyckModRefAnalysis();
    PM.add(DyckMRAPass);
    
    // Run the passes
    PM.run(*M);
    
    outs() << "Creating Dyck Value Flow Graph...\n";
    DyckVFG VFG(DyckAAPass, DyckMRAPass, M.get());
    
    // Create Global Value Flow Analysis
    outs() << "Creating Global Value Flow Analysis...\n";
    DyckGlobalValueFlowAnalysis GVFA(M.get(), &VFG, DyckAAPass, DyckMRAPass);
    
    // Set vulnerability checker based on command line option
    std::unique_ptr<VulnerabilityChecker> checker;
    if (VulnType == "nullpointer") {
        outs() << "Using Null Pointer Vulnerability Checker\n";
        checker = std::make_unique<NullPointerVulnerabilityChecker>();
    } else if (VulnType == "taint") {
        outs() << "Using Taint Vulnerability Checker\n";
        checker = std::make_unique<TaintVulnerabilityChecker>();
    } else {
        errs() << "Unknown vulnerability type: " << VulnType << "\n";
        return 1;
    }
    
    GVFA.setVulnerabilityChecker(std::move(checker));
    
    // Run the analysis
    outs() << "Running Global Value Flow Analysis...\n";
    GVFA.run();
    
    // Perform vulnerability detection by querying reachability
    outs() << "Performing vulnerability detection...\n";
    int vulnerabilitiesFound = 0;
    
    // Get sources and sinks for querying
    VulnerabilitySourcesType QuerySources;
    VulnerabilitySinksType QuerySinks;
    GVFA.getVulnerabilityChecker()->getSources(M.get(), QuerySources);
    GVFA.getVulnerabilityChecker()->getSinks(M.get(), QuerySinks);
    
    // Check reachability from each source to each sink
    for (const auto &SinkPair : QuerySinks) {
        const Value *SinkValue = SinkPair.first;
        const std::set<const Value *> *SinkInsts = SinkPair.second;
        
        for (const auto &SourcePair : QuerySources) {
            const Value *SourceValue = SourcePair.first.first;
            int SourceMask = SourcePair.second;
            
            // Query if sink is reachable from source
            if (GVFA.reachable(SinkValue, SourceMask)) {
                vulnerabilitiesFound++;
                outs() << "VULNERABILITY FOUND: ";
                outs() << GVFA.getVulnerabilityChecker()->getCategory() << " vulnerability detected!\n";
                outs() << "  Source: ";
                SourceValue->print(outs());
                outs() << "\n  Sink: ";
                SinkValue->print(outs());
                outs() << "\n";
                
                // Show associated sink instructions
                for (const Value *SinkInst : *SinkInsts) {
                    outs() << "  At instruction: ";
                    SinkInst->print(outs());
                    outs() << "\n";
                }
                outs() << "\n";
            }
        }
        
        // Also check backward reachability for sinks
        if (GVFA.backwardReachable(SinkValue)) {
            // This sink is reachable from some source
        }
    }
    
    outs() << "Vulnerability detection completed. Found " << vulnerabilitiesFound << " potential vulnerabilities.\n";
    
    // Print statistics if requested
    if (DumpStats) {
        outs() << "\n=== Analysis Statistics ===\n";
        outs() << "Total queries: " << GVFA.AllQueryCounter << "\n";
        outs() << "Successful queries: " << GVFA.SuccsQueryCounter << "\n";
        if (GVFA.AllQueryCounter > 0) {
            double successRate = (double)GVFA.SuccsQueryCounter / GVFA.AllQueryCounter * 100.0;
            outs() << "Success rate: " << successRate << "%\n";
        }
        GVFA.printOnlineQueryTime(outs(), "Online Query");
    }
    
    outs() << "Analysis completed successfully!\n";
    return 0;
} 