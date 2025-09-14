/**
 * @file llvm-ai.cpp
 * @brief LLVM Abstract Interpreter Tool
 * 
 * A command-line tool for running abstract interpretation on LLVM bitcode files.
 * This tool demonstrates the use of the Sparta framework for LLVM IR analysis.
 */

#include <Analysis/sparta/LLVMAbstractInterpreter.h>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/ErrorOr.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>

#include <iostream>
#include <memory>
#include <string>

using namespace llvm;
using namespace sparta::llvm_ai;

static cl::opt<std::string> InputFilename(cl::Positional, cl::desc("<input bitcode file>"),
                                          cl::Required);

static cl::opt<bool> Verbose("verbose", cl::desc("Enable verbose output"), cl::init(false));

static cl::opt<std::string> FunctionName("function", cl::desc("Analyze specific function only"),
                                         cl::init(""));

static cl::opt<bool> ShowCFG("show-cfg", cl::desc("Show control flow graph"), cl::init(false));

static cl::opt<bool> ShowAbstractStates("show-states", cl::desc("Show abstract states at each program point"),
                                        cl::init(false));

int main(int argc, char **argv) {
    InitLLVM X(argc, argv);
    
    cl::ParseCommandLineOptions(argc, argv, "LLVM Abstract Interpreter Tool\n");
    
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
    
    // Create the abstract interpreter
    LLVMAbstractInterpreter Interpreter;
    
    // Analyze functions
    int analyzedFunctions = 0;
    for (Function &F : *M) {
        if (F.isDeclaration()) {
            if (Verbose) {
                outs() << "Skipping declaration: " << F.getName() << "\n";
            }
            continue;
        }
        
        // If a specific function is requested, skip others
        if (!FunctionName.empty() && F.getName() != FunctionName) {
            continue;
        }
        
        if (Verbose) {
            outs() << "Analyzing function: " << F.getName() << "\n";
        }
        
        try {
            // Run abstract interpretation
            Interpreter.analyze_function(&F);
            
            if (ShowAbstractStates) {
                outs() << "Abstract states for function " << F.getName() << ":\n";
                Interpreter.print_analysis_results(&F, std::cout);
            }
            
            if (ShowCFG) {
                outs() << "Control flow graph for function " << F.getName() << ":\n";
                // Print basic block information
                for (const BasicBlock &BB : F) {
                    outs() << "  Basic Block: " << BB.getName() << "\n";
                    outs() << "    Predecessors: ";
                    for (const BasicBlock *Pred : predecessors(&BB)) {
                        outs() << Pred->getName() << " ";
                    }
                    outs() << "\n    Successors: ";
                    for (const BasicBlock *Succ : successors(&BB)) {
                        outs() << Succ->getName() << " ";
                    }
                    outs() << "\n";
                }
            }
            
            // Print analysis results
            outs() << "Function: " << F.getName() << "\n";
            outs() << "  Basic blocks: " << F.size() << "\n";
            outs() << "  Instructions: " << F.getInstructionCount() << "\n";
            outs() << "  Analysis completed successfully\n";
            
            analyzedFunctions++;
            
        } catch (const std::exception &e) {
            errs() << "Error analyzing function " << F.getName() << ": " << e.what() << "\n";
            return 1;
        }
    }
    
    if (analyzedFunctions == 0) {
        if (!FunctionName.empty()) {
            errs() << "Function '" << FunctionName << "' not found in module\n";
        } else {
            errs() << "No functions to analyze in module\n";
        }
        return 1;
    }
    
    outs() << "Analysis completed. Analyzed " << analyzedFunctions << " function(s).\n";
    
    return 0;
}
