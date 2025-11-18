/**
 * @file BugDetection.h
 * @brief Data structures for bug detection and reporting in Miri-like emulator

 */

#ifndef ANALYSIS_LLVMIR_EMUL_BUGDETECTION_H
#define ANALYSIS_LLVMIR_EMUL_BUGDETECTION_H

#include <llvm/IR/Instruction.h>
#include <llvm/IR/Function.h>
#include <string>
#include <vector>
#include <map>

namespace miri {

/**
 * Type of bug detected during concrete execution
 */
enum class BugType {
    BufferOverflow,           // Out-of-bounds write
    BufferUnderflow,          // Out-of-bounds read/write (before buffer)
    UseAfterFree,             // Access to freed memory
    DoubleFree,               // Freeing already freed memory
    NullPointerDeref,         // Dereferencing null pointer
    UninitializedRead,        // Reading uninitialized memory
    DivisionByZero,           // Division or modulo by zero
    SignedIntegerOverflow,    // Signed integer overflow (UB in C)
    SignedIntegerUnderflow,   // Signed integer underflow (UB in C)
    InvalidShift,             // Shift by >= bit width or negative
    InvalidPointerArithmetic, // Pointer arithmetic out of object bounds
    InvalidFree,              // Free of non-heap pointer or invalid pointer
    StackUseAfterReturn,      // Use of stack memory after function return
    MemoryLeak                // Allocated memory not freed (future)
};

/**
 * Convert bug type to human-readable string
 */
inline const char* bugTypeToString(BugType type) {
    switch (type) {
        case BugType::BufferOverflow: return "Buffer Overflow";
        case BugType::BufferUnderflow: return "Buffer Underflow";
        case BugType::UseAfterFree: return "Use After Free";
        case BugType::DoubleFree: return "Double Free";
        case BugType::NullPointerDeref: return "Null Pointer Dereference";
        case BugType::UninitializedRead: return "Uninitialized Read";
        case BugType::DivisionByZero: return "Division By Zero";
        case BugType::SignedIntegerOverflow: return "Signed Integer Overflow";
        case BugType::SignedIntegerUnderflow: return "Signed Integer Underflow";
        case BugType::InvalidShift: return "Invalid Shift";
        case BugType::InvalidPointerArithmetic: return "Invalid Pointer Arithmetic";
        case BugType::InvalidFree: return "Invalid Free";
        case BugType::StackUseAfterReturn: return "Stack Use After Return";
        case BugType::MemoryLeak: return "Memory Leak";
        default: return "Unknown Bug";
    }
}

/**
 * Context information for a detected bug
 * Contains concrete values and execution state
 */
struct BugContext {
    // Concrete values involved in the bug
    std::map<std::string, uint64_t> concrete_values;
    
    // Memory-related context (for memory bugs)
    uint64_t access_addr = 0;      // Address being accessed
    size_t access_size = 0;        // Size of access
    uint64_t region_base = 0;      // Base of memory region
    size_t region_size = 0;        // Size of memory region
    bool is_write = false;         // Write vs read access
    
    // Allocation/free sites (for memory bugs)
    llvm::Instruction* alloc_site = nullptr;
    llvm::Instruction* free_site = nullptr;
    
    // Execution trace (last N instructions before bug)
    std::vector<llvm::Instruction*> trace;
    
    // Call stack at bug location
    std::vector<llvm::Function*> call_stack;
    
    // Additional textual details
    std::string additional_info;
    
    /**
     * Add a concrete value to the context
     */
    void addValue(const std::string& name, uint64_t value) {
        concrete_values[name] = value;
    }
    
    /**
     * Set memory access context
     */
    void setMemoryAccess(uint64_t addr, size_t size, bool write) {
        access_addr = addr;
        access_size = size;
        is_write = write;
    }
    
    /**
     * Set memory region context
     */
    void setMemoryRegion(uint64_t base, size_t size) {
        region_base = base;
        region_size = size;
    }
};

/**
 * A detected bug with all relevant information
 */
struct DetectedBug {
    BugType type;
    llvm::Instruction* location;
    std::string message;
    BugContext context;
    
    // Severity: 0-10, higher = more severe
    int severity;
    
    // Confidence: 0-100, how certain we are this is a real bug
    int confidence;
    
    DetectedBug(BugType t, llvm::Instruction* loc, const std::string& msg)
        : type(t), location(loc), message(msg), severity(5), confidence(100) {}
    
    DetectedBug(BugType t, llvm::Instruction* loc, const std::string& msg, 
                const BugContext& ctx, int sev = 5, int conf = 100)
        : type(t), location(loc), message(msg), context(ctx), 
          severity(sev), confidence(conf) {}
    
    /**
     * Get bug type as string
     */
    const char* getTypeString() const {
        return bugTypeToString(type);
    }
    
    /**
     * Build detailed message including context
     */
    std::string getDetailedMessage() const;
};

/**
 * Severity levels for different bug types
 */
inline int getBugSeverity(BugType type) {
    switch (type) {
        // Critical security issues
        case BugType::BufferOverflow:
        case BugType::UseAfterFree:
        case BugType::DoubleFree:
            return 10;
        
        // High severity
        case BugType::NullPointerDeref:
        case BugType::BufferUnderflow:
        case BugType::InvalidFree:
        case BugType::StackUseAfterReturn:
            return 8;
        
        // Medium severity
        case BugType::UninitializedRead:
        case BugType::SignedIntegerOverflow:
        case BugType::InvalidPointerArithmetic:
            return 6;
        
        // Lower severity
        case BugType::DivisionByZero:
        case BugType::InvalidShift:
        case BugType::SignedIntegerUnderflow:
            return 5;
        
        // Info level
        case BugType::MemoryLeak:
            return 3;
        
        default:
            return 5;
    }
}

} // namespace miri

#endif // ANALYSIS_LLVMIR_EMUL_BUGDETECTION_H

