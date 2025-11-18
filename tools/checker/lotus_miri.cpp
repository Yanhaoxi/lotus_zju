/**
 * @file lotus_miri.cpp
 * @brief Lotus-Miri: Concrete Execution Bug Finder

 * 
 * A Miri-like bug finder that uses concrete execution to detect:
 * - Memory safety bugs (buffer overflow, use-after-free, null deref)
 * - Undefined behavior (division by zero, invalid shifts, integer overflow)
 */

#include "Analysis/llvmir-emul/MiriEmulator.h"
#include "Apps/Checker/Report/BugReportMgr.h"
#include "Apps/Checker/Report/ReportOptions.h"

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

#include <memory>
#include <string>

using namespace llvm;

//=============================================================================
// Command-line Options
//=============================================================================

static cl::opt<std::string> InputFilename(cl::Positional,
    cl::desc("<input bitcode file>"),
    cl::Required);

static cl::opt<std::string> EntryFunction("entry-function",
    cl::desc("Entry function to analyze (default: main)"),
    cl::init("main"));

// Memory safety checks
static cl::opt<bool> CheckBufferOverflow("check-buffer-overflow",
    cl::desc("Enable buffer overflow detection"),
    cl::init(true));

static cl::opt<bool> CheckUseAfterFree("check-use-after-free",
    cl::desc("Enable use-after-free detection"),
    cl::init(true));

static cl::opt<bool> CheckNullDeref("check-null-deref",
    cl::desc("Enable null pointer dereference detection"),
    cl::init(true));

static cl::opt<bool> CheckUninitRead("check-uninit-read",
    cl::desc("Enable uninitialized memory read detection"),
    cl::init(true));

static cl::opt<bool> CheckDoubleFree("check-double-free",
    cl::desc("Enable double-free detection"),
    cl::init(true));

static cl::opt<bool> CheckInvalidFree("check-invalid-free",
    cl::desc("Enable invalid free detection"),
    cl::init(true));

// Undefined behavior checks
static cl::opt<bool> CheckDivByZero("check-div-by-zero",
    cl::desc("Enable division by zero detection"),
    cl::init(true));

static cl::opt<bool> CheckInvalidShift("check-invalid-shift",
    cl::desc("Enable invalid shift detection"),
    cl::init(true));

static cl::opt<bool> CheckIntOverflow("check-int-overflow",
    cl::desc("Enable signed integer overflow detection"),
    cl::init(true));

// Global enable/disable
static cl::opt<bool> CheckAll("check-all",
    cl::desc("Enable all checks"),
    cl::init(false));

static cl::opt<bool> CheckNone("check-none",
    cl::desc("Disable all checks (for testing)"),
    cl::init(false));

// Execution control
static cl::opt<bool> AbortOnFirstError("abort-on-error",
    cl::desc("Stop execution on first error detected"),
    cl::init(false));

static cl::opt<unsigned> MaxErrors("max-errors",
    cl::desc("Maximum number of errors to report"),
    cl::init(100));

static cl::opt<unsigned> MaxInstructions("max-instructions",
    cl::desc("Maximum instructions to execute (prevents infinite loops)"),
    cl::init(1000000));

// Reporting options
static cl::opt<bool> Verbose("verbose",
    cl::desc("Enable verbose output"),
    cl::init(false));

static cl::opt<bool> QuietMode("quiet",
    cl::desc("Suppress all output except final summary"),
    cl::init(false));

static cl::opt<std::string> ReportJson("report-json",
    cl::desc("Output JSON report file"),
    cl::value_desc("filename"));

static cl::opt<std::string> ReportSarif("report-sarif",
    cl::desc("Output SARIF report file"),
    cl::value_desc("filename"));

static cl::opt<int> MinScore("min-score",
    cl::desc("Minimum confidence score to report (0-100)"),
    cl::init(0));

// Statistics
static cl::opt<bool> PrintStats("print-stats",
    cl::desc("Print execution statistics"),
    cl::init(false));

//=============================================================================
// Main Implementation
//=============================================================================

int main(int argc, char** argv) {
    InitLLVM X(argc, argv);
    
    // Initialize command-line options
    report_options::initializeReportOptions();
    
    cl::ParseCommandLineOptions(argc, argv,
        "Lotus-Miri: Concrete Execution Bug Finder\n\n"
        "Detects memory safety bugs and undefined behavior through\n"
        "concrete execution of LLVM IR.\n");
    
    // Load LLVM module
    LLVMContext context;
    SMDiagnostic err;
    std::unique_ptr<Module> module = parseIRFile(InputFilename, err, context);
    
    if (!module) {
        errs() << "Error loading module: " << InputFilename << "\n";
        err.print(argv[0], errs());
        return 1;
    }
    
    if (!QuietMode) {
        outs() << "Loaded module: " << InputFilename << "\n";
        outs() << "Functions: " << module->getFunctionList().size() << "\n\n";
    }
    
    // Find entry function
    Function* entry = module->getFunction(EntryFunction);
    if (!entry) {
        errs() << "Error: Entry function '" << EntryFunction << "' not found\n";
        errs() << "Available functions:\n";
        for (auto& F : *module) {
            if (!F.isDeclaration()) {
                errs() << "  " << F.getName() << "\n";
            }
        }
        return 1;
    }
    
    if (!QuietMode) {
        outs() << "Entry function: " << entry->getName() << "\n";
    }
    
    // Configure emulator
    miri::MiriConfig config;
    
    // Apply global flags
    if (CheckAll) {
        config.enableAll();
    } else if (CheckNone) {
        config.disableAll();
    } else {
        // Apply individual check flags
        config.check_buffer_overflow = CheckBufferOverflow;
        config.check_use_after_free = CheckUseAfterFree;
        config.check_null_deref = CheckNullDeref;
        config.check_uninitialized_read = CheckUninitRead;
        config.check_double_free = CheckDoubleFree;
        config.check_invalid_free = CheckInvalidFree;
        config.check_division_by_zero = CheckDivByZero;
        config.check_invalid_shift = CheckInvalidShift;
        config.check_integer_overflow = CheckIntOverflow;
    }
    
    // Set execution control
    config.abort_on_first_error = AbortOnFirstError;
    config.max_errors = MaxErrors;
    config.max_instructions = MaxInstructions;
    config.verbose = Verbose && !QuietMode;
    config.report_to_manager = true;
    
    if (!QuietMode && config.verbose) {
        outs() << "Configuration:\n";
        outs() << "  Buffer overflow: " << (config.check_buffer_overflow ? "ON" : "OFF") << "\n";
        outs() << "  Use-after-free: " << (config.check_use_after_free ? "ON" : "OFF") << "\n";
        outs() << "  Null dereference: " << (config.check_null_deref ? "ON" : "OFF") << "\n";
        outs() << "  Uninitialized read: " << (config.check_uninitialized_read ? "ON" : "OFF") << "\n";
        outs() << "  Division by zero: " << (config.check_division_by_zero ? "ON" : "OFF") << "\n";
        outs() << "  Integer overflow: " << (config.check_integer_overflow ? "ON" : "OFF") << "\n";
        outs() << "  Invalid shift: " << (config.check_invalid_shift ? "ON" : "OFF") << "\n\n";
    }
    
    // Create emulator
    if (!QuietMode) {
        outs() << "Starting concrete execution...\n";
    }
    
    miri::MiriEmulator emulator(module.get(), config);
    
    // Run function
    try {
        // TODO: Add support for input generation
        // For now, run with default-initialized arguments
        std::vector<GenericValue> args;
        for (auto& arg : entry->args()) {
            GenericValue gv;
            // Initialize to zero
            if (arg.getType()->isIntegerTy()) {
                gv.IntVal = APInt(arg.getType()->getIntegerBitWidth(), 0);
            } else if (arg.getType()->isPointerTy()) {
                gv.PointerVal = nullptr;
            }
            args.push_back(gv);
        }
        
        GenericValue result = emulator.runFunction(entry, args);
        
        if (!QuietMode) {
            outs() << "\nExecution completed.\n";
        }
        
    } catch (const std::exception& e) {
        errs() << "Error during execution: " << e.what() << "\n";
        return 1;
    }
    
    // Print statistics
    if (PrintStats || (!QuietMode && Verbose)) {
        outs() << "\nExecution Statistics:\n";
        const auto& stats = emulator.getStatistics();
        outs() << "  Instructions executed: " << stats.num_instructions_executed << "\n";
        outs() << "  Memory accesses: " << stats.num_memory_accesses << "\n";
        outs() << "  Allocations: " << stats.num_allocations << "\n";
        outs() << "  Frees: " << stats.num_frees << "\n";
        outs() << "  Bugs detected: " << stats.num_bugs_detected << "\n\n";
    }
    
    // Report bugs
    emulator.reportBugs();
    
    // Get bug report manager
    BugReportMgr& bug_mgr = BugReportMgr::get_instance();
    
    // Generate reports
    if (!ReportJson.empty()) {
        std::error_code EC;
        raw_fd_ostream json_out(ReportJson, EC);
        if (!EC) {
            bug_mgr.generate_json_report(json_out, MinScore);
            if (!QuietMode) {
                outs() << "JSON report written to: " << ReportJson << "\n";
            }
        } else {
            errs() << "Error writing JSON report: " << EC.message() << "\n";
        }
    }
    
    if (!ReportSarif.empty()) {
        // TODO: Implement SARIF export
        errs() << "SARIF export not yet implemented\n";
    }
    
    // Print summary
    if (!QuietMode) {
        outs() << "\n";
        outs() << "========================================\n";
        outs() << "Bug Detection Summary\n";
        outs() << "========================================\n";
        bug_mgr.print_summary(outs());
    }
    
    int total_bugs = bug_mgr.get_total_reports();
    
    if (!QuietMode) {
        if (total_bugs == 0) {
            outs() << "\n✓ No bugs detected!\n";
        } else {
            outs() << "\n✗ Found " << total_bugs << " bug(s)\n";
        }
    }
    
    // Return non-zero if bugs were found
    return (total_bugs > 0) ? 1 : 0;
}

