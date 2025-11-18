/**
 * @file MiriEmulator.cpp
 * @brief Implementation of Miri-like bug finder
 * @copyright (c) 2024 Lotus Project
 */

#include "Analysis/llvmir-emul/MiriEmulator.h"
#include "Apps/Checker/Report/BugReportMgr.h"
#include "Apps/Checker/Report/BugReport.h"
#include "Apps/Checker/Report/BugTypes.h"

#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/Support/raw_ostream.h>

namespace miri {

//=============================================================================
// MiriEmulator Implementation
//=============================================================================

MiriEmulator::MiriEmulator(llvm::Module* module, const MiriConfig& config)
    : LlvmIrEmulator(module), config_(config), memory_model_() {
    // Store references to base class members for accessor methods
    // We'll access them through the base class's public interface where possible
    
    // Initialize global variables in memory model
    const llvm::DataLayout& DL = module->getDataLayout();
    for (llvm::GlobalVariable& gv : module->globals()) {
        if (gv.hasInitializer()) {
            // Compute size of global variable
            llvm::Type* ty = gv.getValueType();
            size_t size = DL.getTypeAllocSize(ty);
            
            // Use the global variable's address as its base
            // In the emulator, we'll use pointer value from GenericValue
            uint64_t addr = reinterpret_cast<uint64_t>(&gv);
            
            memory_model_.registerGlobalVariable(addr, size, &gv);
        }
    }
    
    // Create bug checkers based on configuration
    if (config_.check_buffer_overflow || config_.check_use_after_free ||
        config_.check_null_deref || config_.check_double_free ||
        config_.check_invalid_free) {
        checkers_.push_back(std::make_unique<MemorySafetyChecker>());
    }
    
    if (config_.check_uninitialized_read) {
        checkers_.push_back(std::make_unique<UninitializedMemoryChecker>());
    }
    
    if (config_.check_division_by_zero) {
        checkers_.push_back(std::make_unique<DivisionByZeroChecker>());
    }
    
    if (config_.check_integer_overflow) {
        checkers_.push_back(std::make_unique<IntegerOverflowChecker>());
    }
    
    if (config_.check_invalid_shift) {
        checkers_.push_back(std::make_unique<InvalidShiftChecker>());
    }
}

llvm::GenericValue MiriEmulator::runFunction(
    llvm::Function* f,
    const llvm::ArrayRef<llvm::GenericValue> argVals) {
    
    // Reset statistics
    stats_ = Statistics();
    aborted_ = false;
    
    // Initialize bug types if needed
    if (config_.report_to_manager && !bug_types_initialized_) {
        initializeBugTypes();
    }
    
    // Push initial stack frame
    memory_model_.pushStackFrame();
    
    // Call base implementation
    llvm::GenericValue result = LlvmIrEmulator::runFunction(f, argVals);
    
    // Pop stack frame
    memory_model_.popStackFrameMarker();
    
    // Update statistics
    stats_.num_bugs_detected = detected_bugs_.size();
    
    return result;
}

void MiriEmulator::run() {
    // Override run() to add bug checking hooks
    // Now that _ecStack is protected, we can access it
    while (!_ecStack.empty() && !shouldAbort()) {
        auto& ec = _ecStack.back();
        if (ec.curInst == ec.curBB->end()) {
            break;
        }
        llvm::Instruction& I = *ec.curInst++;
        
        // Call checkers before executing
        callCheckersPreVisit(I);
        
        // Log instruction (base class method)
        logInstruction(&I);
        
        // Execute instruction (base class visitor)
        visit(I);
        
        // Call checkers after executing
        callCheckersPostVisit(I);
        
        // Update statistics
        ++stats_.num_instructions_executed;
        
        // Check if we should abort
        if (config_.abort_on_first_error && !detected_bugs_.empty()) {
            aborted_ = true;
            break;
        }
    }
    
    if (aborted_ && config_.verbose) {
        llvm::errs() << "Execution aborted after detecting " 
                     << detected_bugs_.size() << " bug(s)\n";
    }
}

void MiriEmulator::visitLoadInst(llvm::LoadInst& I) {
    // Checkers are called in run() before/after visit()
    ++stats_.num_memory_accesses;
    
    // Call base implementation
    LlvmIrEmulator::visitLoadInst(I);
}

void MiriEmulator::visitStoreInst(llvm::StoreInst& I) {
    // Checkers are called in run() before/after visit()
    ++stats_.num_memory_accesses;
    
    // Call base implementation
    LlvmIrEmulator::visitStoreInst(I);
}

void MiriEmulator::visitBinaryOperator(llvm::BinaryOperator& I) {
    // Checkers are called in run() before/after visit()
    // Call base implementation
    LlvmIrEmulator::visitBinaryOperator(I);
}

void MiriEmulator::visitCallInst(llvm::CallInst& I) {
    // Checkers are called in run() before/after visit()
    
    llvm::Function* calledFunc = I.getCalledFunction();
    
    if (calledFunc) {
        llvm::StringRef name = calledFunc->getName();
        
        // Handle memory allocation functions
        if (name == "malloc" || name == "calloc" || name == "realloc") {
            // These will be handled by base emulator and memory safety checker
        }
        // Handle memory deallocation
        else if (name == "free") {
            // Handled by memory safety checker
        }
    }
    
    // Push stack frame for function call
    if (calledFunc && !calledFunc->isDeclaration()) {
        memory_model_.pushStackFrame();
    }
    
    // Call base implementation
    LlvmIrEmulator::visitCallInst(I);
}

void MiriEmulator::visitAllocaInst(llvm::AllocaInst& I) {
    // Checkers are called in run() before/after visit()
    
    // Call base implementation first
    LlvmIrEmulator::visitAllocaInst(I);
    
    // Register allocation in memory model (after base class computes address)
    if (hasExecutionContext()) {
        auto& ec = getCurrentExecutionContext();
        auto& globalEc = getGlobalExecutionContext();
        llvm::Type* ty = I.getAllocatedType();
        
        unsigned elemN = globalEc.getOperandValue(I.getOperand(0), ec).IntVal.getZExtValue();
        unsigned tySz = static_cast<size_t>(getModule()->getDataLayout().getTypeAllocSize(ty));
        unsigned memToAlloc = std::max(1U, elemN * tySz);
        
        // Get the allocated address from the result
        llvm::GenericValue allocated = getValueValue(&I);
        uint64_t addr = reinterpret_cast<uint64_t>(GVTOP(allocated));
        
        // Register in memory model
        memory_model_.registerStackAllocation(addr, memToAlloc, &I);
        
        ++stats_.num_allocations;
    }
}

void MiriEmulator::visitReturnInst(llvm::ReturnInst& I) {
    // Checkers are called in run() before/after visit()
    
    // Pop stack frame marker
    memory_model_.popStackFrameMarker();
    
    // Call base implementation
    LlvmIrEmulator::visitReturnInst(I);
}

void MiriEmulator::callCheckersPreVisit(llvm::Instruction& I) {
    for (auto& checker : checkers_) {
        if (checker->isEnabled()) {
            checker->preVisit(I, *this);
        }
    }
}

void MiriEmulator::callCheckersPostVisit(llvm::Instruction& I) {
    for (auto& checker : checkers_) {
        if (checker->isEnabled()) {
            checker->postVisit(I, *this);
        }
    }
}

void MiriEmulator::reportBug(BugType type, llvm::Instruction* location,
                             const std::string& message,
                             const BugContext& context) {
    // Create detected bug
    int severity = getBugSeverity(type);
    DetectedBug bug(type, location, message, context, severity);
    
    // Build execution trace
    bug.context.trace = buildExecutionTrace();
    
    // Build call stack
    bug.context.call_stack = buildCallStack();
    
    // Add to detected bugs
    detected_bugs_.push_back(bug);
    
    if (config_.verbose) {
        llvm::errs() << "Bug detected: " << bugTypeToString(type) << "\n";
        llvm::errs() << "  " << message << "\n";
        llvm::errs() << "  Location: ";
        location->print(llvm::errs());
        llvm::errs() << "\n";
    }
}

void MiriEmulator::reportBugs() {
    if (!config_.report_to_manager || detected_bugs_.empty()) {
        return;
    }
    
    if (!bug_types_initialized_) {
        initializeBugTypes();
    }
    
    BugReportMgr& bug_mgr = BugReportMgr::get_instance();
    
    for (const auto& bug : detected_bugs_) {
        int type_id = getBugTypeId(bug.type);
        if (type_id == -1) {
            continue;  // Unknown bug type
        }
        
        // Create bug report
        auto* report = new BugReport(type_id);
        report->set_conf_score(bug.confidence);
        
        // Add main diagnostic step
        auto* main_step = new BugDiagStep();
        main_step->inst = bug.location;
        main_step->tip = bug.getDetailedMessage();
        
        // Extract function name
        if (bug.location && bug.location->getParent()) {
            llvm::Function* func = bug.location->getParent()->getParent();
            if (func) {
                main_step->func_name = func->getName().str();
            }
        }
        
        // Get LLVM IR string
        std::string ir_str;
        llvm::raw_string_ostream ir_os(ir_str);
        bug.location->print(ir_os);
        main_step->llvm_ir = ir_os.str();
        
        report->append_step(main_step);
        
        // Add allocation site if present
        if (bug.context.alloc_site) {
            auto* alloc_step = new BugDiagStep();
            alloc_step->inst = bug.context.alloc_site;
            alloc_step->tip = "Memory allocated here";
            report->append_step(alloc_step);
        }
        
        // Add free site if present (for use-after-free, double-free)
        if (bug.context.free_site) {
            auto* free_step = new BugDiagStep();
            free_step->inst = bug.context.free_site;
            free_step->tip = "Memory freed here";
            report->append_step(free_step);
        }
        
        // Insert into bug manager
        bug_mgr.insert_report(type_id, report);
    }
}

void MiriEmulator::initializeBugTypes() {
    BugReportMgr& bug_mgr = BugReportMgr::get_instance();
    
    // Register all bug types
    bug_type_ids_[BugType::BufferOverflow] = bug_mgr.register_bug_type(
        "Buffer Overflow",
        BugDescription::BI_HIGH,
        BugDescription::BC_SECURITY,
        "CWE-119: Buffer overflow detected during concrete execution"
    );
    
    bug_type_ids_[BugType::BufferUnderflow] = bug_mgr.register_bug_type(
        "Buffer Underflow",
        BugDescription::BI_HIGH,
        BugDescription::BC_SECURITY,
        "CWE-124: Buffer underflow detected during concrete execution"
    );
    
    bug_type_ids_[BugType::UseAfterFree] = bug_mgr.register_bug_type(
        "Use After Free",
        BugDescription::BI_HIGH,
        BugDescription::BC_SECURITY,
        "CWE-416: Use after free detected"
    );
    
    bug_type_ids_[BugType::DoubleFree] = bug_mgr.register_bug_type(
        "Double Free",
        BugDescription::BI_HIGH,
        BugDescription::BC_SECURITY,
        "CWE-415: Double free detected"
    );
    
    bug_type_ids_[BugType::NullPointerDeref] = bug_mgr.register_bug_type(
        "Null Pointer Dereference",
        BugDescription::BI_HIGH,
        BugDescription::BC_ERROR,
        "CWE-476: Null pointer dereference detected"
    );
    
    bug_type_ids_[BugType::UninitializedRead] = bug_mgr.register_bug_type(
        "Uninitialized Read",
        BugDescription::BI_MEDIUM,
        BugDescription::BC_ERROR,
        "CWE-457: Uninitialized memory read detected"
    );
    
    bug_type_ids_[BugType::DivisionByZero] = bug_mgr.register_bug_type(
        "Division By Zero",
        BugDescription::BI_MEDIUM,
        BugDescription::BC_ERROR,
        "CWE-369: Division by zero detected"
    );
    
    bug_type_ids_[BugType::SignedIntegerOverflow] = bug_mgr.register_bug_type(
        "Signed Integer Overflow",
        BugDescription::BI_MEDIUM,
        BugDescription::BC_ERROR,
        "CWE-190: Signed integer overflow (undefined behavior)"
    );
    
    bug_type_ids_[BugType::SignedIntegerUnderflow] = bug_mgr.register_bug_type(
        "Signed Integer Underflow",
        BugDescription::BI_MEDIUM,
        BugDescription::BC_ERROR,
        "CWE-191: Signed integer underflow (undefined behavior)"
    );
    
    bug_type_ids_[BugType::InvalidShift] = bug_mgr.register_bug_type(
        "Invalid Shift",
        BugDescription::BI_MEDIUM,
        BugDescription::BC_ERROR,
        "CWE-1335: Invalid shift operation"
    );
    
    bug_type_ids_[BugType::InvalidPointerArithmetic] = bug_mgr.register_bug_type(
        "Invalid Pointer Arithmetic",
        BugDescription::BI_MEDIUM,
        BugDescription::BC_ERROR,
        "Pointer arithmetic outside object bounds"
    );
    
    bug_type_ids_[BugType::InvalidFree] = bug_mgr.register_bug_type(
        "Invalid Free",
        BugDescription::BI_HIGH,
        BugDescription::BC_ERROR,
        "CWE-590: Invalid free operation"
    );
    
    bug_type_ids_[BugType::StackUseAfterReturn] = bug_mgr.register_bug_type(
        "Stack Use After Return",
        BugDescription::BI_HIGH,
        BugDescription::BC_ERROR,
        "CWE-562: Use of stack memory after function return"
    );
    
    bug_types_initialized_ = true;
}

int MiriEmulator::getBugTypeId(BugType type) const {
    auto it = bug_type_ids_.find(type);
    return (it != bug_type_ids_.end()) ? it->second : -1;
}

std::vector<llvm::Instruction*> MiriEmulator::buildExecutionTrace(unsigned depth) const {
    std::vector<llvm::Instruction*> trace;
    
    const auto& visited = getVisitedInstructions();
    if (visited.empty()) {
        return trace;
    }
    
    // Get last N instructions
    auto it = visited.rbegin();
    for (unsigned i = 0; i < depth && it != visited.rend(); ++i, ++it) {
        trace.push_back(*it);
    }
    
    // Reverse so it's in execution order
    std::reverse(trace.begin(), trace.end());
    
    return trace;
}

std::vector<llvm::Function*> MiriEmulator::buildCallStack() const {
    std::vector<llvm::Function*> stack;
    
    // Now that _ecStack is protected, we can access it
    for (const auto& ec : _ecStack) {
        if (ec.curFunction) {
            stack.push_back(ec.curFunction);
        }
    }
    
    return stack;
}

bool MiriEmulator::shouldAbort() const {
    if (aborted_) {
        return true;
    }
    
    if (config_.abort_on_first_error && !detected_bugs_.empty()) {
        return true;
    }
    
    if (detected_bugs_.size() >= config_.max_errors) {
        return true;
    }
    
    if (stats_.num_instructions_executed >= config_.max_instructions) {
        return true;
    }
    
    return false;
}

} // namespace miri

