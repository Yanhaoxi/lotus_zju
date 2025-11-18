/**
 * @file BugCheckers.h
 * @brief Bug checker framework for Miri-like emulator

 */

#ifndef ANALYSIS_LLVMIR_EMUL_BUGCHECKERS_H
#define ANALYSIS_LLVMIR_EMUL_BUGCHECKERS_H

#include "Analysis/llvmir-emul/BugDetection.h"
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <string>
#include <memory>

namespace miri {

// Forward declaration
class MiriEmulator;

/**
 * Base class for all bug checkers
 * Each checker implements detection logic for a specific class of bugs
 */
class BugChecker {
public:
    virtual ~BugChecker() = default;
    
    /**
     * Called before executing an instruction
     * Checkers can examine instruction and operands to detect potential bugs
     */
    virtual void preVisit(llvm::Instruction& I, MiriEmulator& emulator) {}
    
    /**
     * Called after executing an instruction
     * Checkers can examine results to detect bugs
     */
    virtual void postVisit(llvm::Instruction& I, MiriEmulator& emulator) {}
    
    /**
     * Get checker name for reporting and debugging
     */
    virtual std::string getName() const = 0;
    
    /**
     * Get checker description
     */
    virtual std::string getDescription() const = 0;
    
    /**
     * Enable/disable this checker
     */
    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool isEnabled() const { return enabled_; }
    
protected:
    bool enabled_ = true;
};

/**
 * Memory safety checker
 * Detects buffer overflows, use-after-free, null dereferences
 */
class MemorySafetyChecker : public BugChecker {
public:
    void preVisit(llvm::Instruction& I, MiriEmulator& emulator) override;
    std::string getName() const override { return "MemorySafety"; }
    std::string getDescription() const override {
        return "Detects buffer overflows, use-after-free, and null pointer dereferences";
    }
    
private:
    void checkLoad(llvm::LoadInst& I, MiriEmulator& emulator);
    void checkStore(llvm::StoreInst& I, MiriEmulator& emulator);
    void checkMemCpy(llvm::CallInst& I, MiriEmulator& emulator);
    void checkMemSet(llvm::CallInst& I, MiriEmulator& emulator);
    void checkFree(llvm::CallInst& I, MiriEmulator& emulator);
    void checkAlloca(llvm::AllocaInst& I, MiriEmulator& emulator);
    
    // Helper to check pointer validity
    void checkPointerAccess(llvm::Instruction& I, MiriEmulator& emulator,
                           uint64_t addr, size_t size, bool is_write,
                           const char* operation);
};

/**
 * Division by zero checker
 * Detects division and modulo operations with zero divisor
 */
class DivisionByZeroChecker : public BugChecker {
public:
    void preVisit(llvm::Instruction& I, MiriEmulator& emulator) override;
    std::string getName() const override { return "DivisionByZero"; }
    std::string getDescription() const override {
        return "Detects division and modulo by zero";
    }
    
private:
    void checkDivision(llvm::BinaryOperator& I, MiriEmulator& emulator);
    bool isZero(const llvm::GenericValue& val, llvm::Type* type);
};

/**
 * Integer overflow checker
 * Detects signed integer overflow (undefined behavior in C/C++)
 */
class IntegerOverflowChecker : public BugChecker {
public:
    IntegerOverflowChecker() = default;
    
    void preVisit(llvm::Instruction& I, MiriEmulator& emulator) override;
    std::string getName() const override { return "IntegerOverflow"; }
    std::string getDescription() const override {
        return "Detects signed integer overflow and underflow";
    }
    
private:
    void checkAddOverflow(llvm::BinaryOperator& I, MiriEmulator& emulator);
    void checkSubOverflow(llvm::BinaryOperator& I, MiriEmulator& emulator);
    void checkMulOverflow(llvm::BinaryOperator& I, MiriEmulator& emulator);
    void checkNegOverflow(llvm::BinaryOperator& I, MiriEmulator& emulator);
    
    // Overflow detection helpers
    bool willSignedAddOverflow(int64_t a, int64_t b, unsigned bits);
    bool willSignedSubOverflow(int64_t a, int64_t b, unsigned bits);
    bool willSignedMulOverflow(int64_t a, int64_t b, unsigned bits);
    
    // Extract signed value from GenericValue
    int64_t getSignedValue(const llvm::GenericValue& val, unsigned bits);
};

/**
 * Invalid shift checker
 * Detects shift operations with invalid shift amounts
 */
class InvalidShiftChecker : public BugChecker {
public:
    void preVisit(llvm::Instruction& I, MiriEmulator& emulator) override;
    std::string getName() const override { return "InvalidShift"; }
    std::string getDescription() const override {
        return "Detects shifts by >= bit width or negative amounts";
    }
    
private:
    void checkShift(llvm::BinaryOperator& I, MiriEmulator& emulator);
    uint64_t getShiftAmount(const llvm::GenericValue& val, llvm::Type* type);
};

/**
 * Null pointer checker
 * Specifically checks for null pointer dereferences
 */
class NullPointerChecker : public BugChecker {
public:
    void preVisit(llvm::Instruction& I, MiriEmulator& emulator) override;
    std::string getName() const override { return "NullPointer"; }
    std::string getDescription() const override {
        return "Detects null pointer dereferences";
    }
    
private:
    void checkLoadStore(llvm::Instruction& I, MiriEmulator& emulator);
    void checkCall(llvm::CallInst& I, MiriEmulator& emulator);
    bool isNullPointer(uint64_t ptr);
    
    static constexpr uint64_t null_threshold_ = 4096;
};

/**
 * Uninitialized memory checker
 * Detects reads from uninitialized memory
 */
class UninitializedMemoryChecker : public BugChecker {
public:
    void preVisit(llvm::Instruction& I, MiriEmulator& emulator) override;
    std::string getName() const override { return "UninitializedMemory"; }
    std::string getDescription() const override {
        return "Detects reads from uninitialized memory";
    }
    
private:
    void checkLoad(llvm::LoadInst& I, MiriEmulator& emulator);
};

/**
 * Factory for creating bug checkers
 */
class BugCheckerFactory {
public:
    /**
     * Create all standard bug checkers
     */
    static std::vector<std::unique_ptr<BugChecker>> createAllCheckers();
    
    /**
     * Create specific checker by name
     */
    static std::unique_ptr<BugChecker> createChecker(const std::string& name);
    
    /**
     * Get list of available checker names
     */
    static std::vector<std::string> getAvailableCheckers();
};

} // namespace miri

#endif // ANALYSIS_LLVMIR_EMUL_BUGCHECKERS_H

