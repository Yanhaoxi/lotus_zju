/**
 * @file MiriEmulator.h
 * @brief Miri-like bug finder built on LlvmIrEmulator

 */

#ifndef ANALYSIS_LLVMIR_EMUL_MIRIEMULATOR_H
#define ANALYSIS_LLVMIR_EMUL_MIRIEMULATOR_H

#include "Analysis/llvmir-emul/llvmir_emul.h"
#include "Analysis/llvmir-emul/MemoryModel.h"
#include "Analysis/llvmir-emul/BugDetection.h"
#include "Analysis/llvmir-emul/BugCheckers.h"

#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/ExecutionEngine/GenericValue.h>

#include <memory>
#include <vector>
#include <map>
#include <string>

namespace miri {
// Forward declare bug checkers for friend access
class BugChecker;
class MemorySafetyChecker;
class DivisionByZeroChecker;
class IntegerOverflowChecker;
class InvalidShiftChecker;
class NullPointerChecker;
class UninitializedMemoryChecker;

/**
 * Configuration for MiriEmulator
 */
struct MiriConfig {
    // Memory safety checks
    bool check_buffer_overflow = true;
    bool check_use_after_free = true;
    bool check_null_deref = true;
    bool check_uninitialized_read = true;
    bool check_double_free = true;
    bool check_invalid_free = true;
    
    // Undefined behavior checks
    bool check_division_by_zero = true;
    bool check_invalid_shift = true;
    bool check_integer_overflow = true;
    
    // Execution control
    bool abort_on_first_error = false;
    unsigned max_errors = 100;
    unsigned max_instructions = 1000000;  // Prevent infinite loops
    
    // Reporting
    bool verbose = false;
    bool report_to_manager = true;  // Report to BugReportMgr
    
    // Null pointer threshold
    uint64_t null_pointer_threshold = 4096;
    
    /**
     * Enable all checks
     */
    void enableAll() {
        check_buffer_overflow = true;
        check_use_after_free = true;
        check_null_deref = true;
        check_uninitialized_read = true;
        check_double_free = true;
        check_invalid_free = true;
        check_division_by_zero = true;
        check_invalid_shift = true;
        check_integer_overflow = true;
    }
    
    /**
     * Disable all checks
     */
    void disableAll() {
        check_buffer_overflow = false;
        check_use_after_free = false;
        check_null_deref = false;
        check_uninitialized_read = false;
        check_double_free = false;
        check_invalid_free = false;
        check_division_by_zero = false;
        check_invalid_shift = false;
        check_integer_overflow = false;
    }
};

/**
 * Miri-like emulator that extends LlvmIrEmulator with bug detection
 */
class MiriEmulator : public retdec::llvmir_emul::LlvmIrEmulator {
    // Friend classes for accessing protected/private members
    friend class miri::BugChecker;
    friend class miri::MemorySafetyChecker;
    friend class miri::DivisionByZeroChecker;
    friend class miri::IntegerOverflowChecker;
    friend class miri::InvalidShiftChecker;
    friend class miri::NullPointerChecker;
    friend class miri::UninitializedMemoryChecker;
    
    // Accessor methods for bug checkers
    // Now that base class members are protected, we can access them directly
public:
    // Get execution context (for bug checkers)
    retdec::llvmir_emul::LocalExecutionContext& getCurrentExecutionContext() {
        return _ecStack.back();
    }
    
    const retdec::llvmir_emul::LocalExecutionContext& getCurrentExecutionContext() const {
        return _ecStack.back();
    }
    
    bool hasExecutionContext() const {
        return !_ecStack.empty();
    }
    
    // Get global execution context
    retdec::llvmir_emul::GlobalExecutionContext& getGlobalExecutionContext() {
        return _globalEc;
    }
    
    const retdec::llvmir_emul::GlobalExecutionContext& getGlobalExecutionContext() const {
        return _globalEc;
    }
    
    // Get module
    llvm::Module* getModule() const {
        return _module;
    }
    
public:
    /**
     * Constructor
     * @param module LLVM module to analyze
     * @param config Configuration for bug detection
     */
    explicit MiriEmulator(llvm::Module* module, const MiriConfig& config = MiriConfig());
    
    ~MiriEmulator() = default;
    
    /**
     * Run function with concrete inputs and detect bugs
     * @param f Function to execute
     * @param argVals Argument values
     * @return Return value of function
     */
    llvm::GenericValue runFunction(
        llvm::Function* f,
        const llvm::ArrayRef<llvm::GenericValue> argVals = {});
    
    /**
     * Get all detected bugs
     */
    const std::vector<DetectedBug>& getDetectedBugs() const {
        return detected_bugs_;
    }
    
    /**
     * Get number of detected bugs
     */
    size_t getNumBugs() const { return detected_bugs_.size(); }
    
    /**
     * Check if any bugs were detected
     */
    bool hasBugs() const { return !detected_bugs_.empty(); }
    
    /**
     * Report all detected bugs to BugReportMgr
     */
    void reportBugs();
    
    /**
     * Get memory model
     */
    MemoryModel& getMemoryModel() { return memory_model_; }
    const MemoryModel& getMemoryModel() const { return memory_model_; }
    
    /**
     * Get configuration
     */
    const MiriConfig& getConfig() const { return config_; }
    
    /**
     * Add a bug checker
     */
    void addChecker(std::unique_ptr<BugChecker> checker) {
        checkers_.push_back(std::move(checker));
    }
    
    /**
     * Get all checkers
     */
    const std::vector<std::unique_ptr<BugChecker>>& getCheckers() const {
        return checkers_;
    }
    
    /**
     * Report a detected bug
     * Called by bug checkers when they find an issue
     */
    void reportBug(BugType type, llvm::Instruction* location,
                  const std::string& message,
                  const BugContext& context = BugContext());
    
    /**
     * Clear all detected bugs (for testing)
     */
    void clearBugs() { detected_bugs_.clear(); }
    
    /**
     * Get execution statistics
     */
    struct Statistics {
        size_t num_instructions_executed = 0;
        size_t num_memory_accesses = 0;
        size_t num_allocations = 0;
        size_t num_frees = 0;
        size_t num_bugs_detected = 0;
    };
    
    const Statistics& getStatistics() const { return stats_; }
    
    // Override visitor methods to add bug checking
protected:
    void visitLoadInst(llvm::LoadInst& I);
    void visitStoreInst(llvm::StoreInst& I);
    void visitBinaryOperator(llvm::BinaryOperator& I);
    void visitCallInst(llvm::CallInst& I);
    void visitAllocaInst(llvm::AllocaInst& I);
    void visitReturnInst(llvm::ReturnInst& I);
    
private:
    /**
     * Run method to add instruction counting
     */
    void run();
    
    /**
     * Call all checkers before visiting instruction
     */
    void callCheckersPreVisit(llvm::Instruction& I);
    
    /**
     * Call all checkers after visiting instruction
     */
    void callCheckersPostVisit(llvm::Instruction& I);
    
    /**
     * Initialize bug type IDs with BugReportMgr
     */
    void initializeBugTypes();
    
    /**
     * Get bug type ID for BugReportMgr
     */
    int getBugTypeId(BugType type) const;
    
    /**
     * Build execution trace for bug context
     */
    std::vector<llvm::Instruction*> buildExecutionTrace(unsigned depth = 10) const;
    
    /**
     * Build call stack for bug context
     */
    std::vector<llvm::Function*> buildCallStack() const;
    
    /**
     * Check if we should abort (hit max errors or max instructions)
     */
    bool shouldAbort() const;
    
    /**
     * Internal helper to get execution context
     * Uses a workaround to access base class private members
     */
    retdec::llvmir_emul::LocalExecutionContext* getExecutionContextPtr();
    const retdec::llvmir_emul::LocalExecutionContext* getExecutionContextPtr() const;
    
private:
    MiriConfig config_;
    MemoryModel memory_model_;
    std::vector<std::unique_ptr<BugChecker>> checkers_;
    std::vector<DetectedBug> detected_bugs_;
    Statistics stats_;
    
    // Map BugType to BugReportMgr type ID
    std::map<BugType, int> bug_type_ids_;
    
    // Track whether bug types have been registered
    bool bug_types_initialized_ = false;
    
    // Execution control
    bool aborted_ = false;
};

} // namespace miri

#endif // ANALYSIS_LLVMIR_EMUL_MIRIEMULATOR_H

