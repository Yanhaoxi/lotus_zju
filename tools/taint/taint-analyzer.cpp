// #include <iostream>
#include <memory>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include "Checker/Taint/TaintAnalysis.h"
#include "Checker/Taint/TaintUtils.h"

using namespace llvm;
using namespace taint;

// Command line options
static cl::opt<std::string> InputFilename(cl::Positional,
                                        cl::desc("<input bitcode file>"),
                                        cl::Required);

static cl::opt<std::string> TaintConfigFile("config",
                                     cl::desc("Taint configuration file"),
                                     cl::value_desc("filename"),
                                     cl::init(""));

static cl::opt<std::string> OutputFile("output",
                                      cl::desc("Output file for results"),
                                      cl::value_desc("filename"),
                                      cl::init(""));

int main(int argc, char** argv) {
    cl::ParseCommandLineOptions(argc, argv, "LLVM Taint Analysis Tool\n");
    
    // Initialize LLVM
    LLVMContext context;
    SMDiagnostic error;
    
    // Load the LLVM module
    std::unique_ptr<Module> module = parseIRFile(InputFilename, error, context);
    if (!module) {
        errs() << "Error loading module: " << error.getMessage() << "\n";
        return 1;
    }
    
    outs() << "Loaded module: " << module->getName() << "\n";
    
    // Create taint analysis configuration
    TaintConfig config;
    config.trackThroughMemory = true;
    config.trackThroughCalls = true;
    config.maxCallDepth = 5;
    
    // Load custom configuration if provided
    if (!TaintConfigFile.empty()) {
        outs() << "Loading config: " << TaintConfigFile << "\n";
        TaintUtils::loadConfigFromFile(TaintConfigFile, 
                                     config.sourceFunctions, 
                                     config.sinkFunctions);
    }
    
    // Run taint analysis
    outs() << "Running taint analysis...\n";
    TaintAnalysis analyzer(config);
    analyzer.analyzeModule(module.get());
    
    // Output results
    const auto& result = analyzer.getResult();
    
    if (OutputFile.empty()) {
        result.printResults(outs());
    } else {
        std::error_code EC;
        raw_fd_ostream outFile(OutputFile, EC);
        if (EC) {
            errs() << "Error opening output file: " << EC.message() << "\n";
            return 1;
        }
        result.printResults(outFile);
        outs() << "Results written to: " << OutputFile << "\n";
    }
    
    return 0;
} 