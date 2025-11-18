/**
 * @file IntegerOverflowChecker.cpp
 * @brief Implementation of integer overflow checker
 */

#include "Analysis/llvmir-emul/BugCheckers.h"
#include "Analysis/llvmir-emul/MiriEmulator.h"
#include <llvm/IR/Instructions.h>
#include <llvm/Support/MathExtras.h>
#include <sstream>
#include <climits>

namespace miri {

void IntegerOverflowChecker::preVisit(llvm::Instruction& I, MiriEmulator& emulator) {
    if (auto* BO = llvm::dyn_cast<llvm::BinaryOperator>(&I)) {
        unsigned opcode = BO->getOpcode();
        
        // Only check signed operations (signed overflow is UB in C/C++)
        // Unsigned overflow is well-defined (wraps around)
        switch (opcode) {
            case llvm::Instruction::Add:
                checkAddOverflow(*BO, emulator);
                break;
            case llvm::Instruction::Sub:
                checkSubOverflow(*BO, emulator);
                break;
            case llvm::Instruction::Mul:
                checkMulOverflow(*BO, emulator);
                break;
            default:
                break;
        }
    }
}

void IntegerOverflowChecker::checkAddOverflow(llvm::BinaryOperator& I, MiriEmulator& emulator) {
    if (!emulator.hasExecutionContext() || !I.getType()->isIntegerTy()) {
        return;
    }
    
    auto& ec = emulator.getCurrentExecutionContext();
    auto& globalEc = emulator.getGlobalExecutionContext();
    
    // Get operands
    llvm::GenericValue op1Val = globalEc.getOperandValue(I.getOperand(0), ec);
    llvm::GenericValue op2Val = globalEc.getOperandValue(I.getOperand(1), ec);
    
    unsigned bits = I.getType()->getIntegerBitWidth();
    int64_t a = getSignedValue(op1Val, bits);
    int64_t b = getSignedValue(op2Val, bits);
    
    if (willSignedAddOverflow(a, b, bits)) {
        BugContext ctx;
        ctx.addValue("operand1", static_cast<uint64_t>(a));
        ctx.addValue("operand2", static_cast<uint64_t>(b));
        
        std::ostringstream oss;
        oss << "Signed integer overflow in addition: " << a << " + " << b
            << " (bit width=" << bits << ")";
        
        emulator.reportBug(BugType::SignedIntegerOverflow, &I, oss.str(), ctx);
    }
}

void IntegerOverflowChecker::checkSubOverflow(llvm::BinaryOperator& I, MiriEmulator& emulator) {
    if (!emulator.hasExecutionContext() || !I.getType()->isIntegerTy()) {
        return;
    }
    
    auto& ec = emulator.getCurrentExecutionContext();
    auto& globalEc = emulator.getGlobalExecutionContext();
    
    // Get operands
    llvm::GenericValue op1Val = globalEc.getOperandValue(I.getOperand(0), ec);
    llvm::GenericValue op2Val = globalEc.getOperandValue(I.getOperand(1), ec);
    
    unsigned bits = I.getType()->getIntegerBitWidth();
    int64_t a = getSignedValue(op1Val, bits);
    int64_t b = getSignedValue(op2Val, bits);
    
    if (willSignedSubOverflow(a, b, bits)) {
        BugContext ctx;
        ctx.addValue("operand1", static_cast<uint64_t>(a));
        ctx.addValue("operand2", static_cast<uint64_t>(b));
        
        std::ostringstream oss;
        oss << "Signed integer overflow/underflow in subtraction: " << a << " - " << b
            << " (bit width=" << bits << ")";
        
        BugType type = (a < 0 && b > 0) ? BugType::SignedIntegerUnderflow
                                         : BugType::SignedIntegerOverflow;
        
        emulator.reportBug(type, &I, oss.str(), ctx);
    }
}

void IntegerOverflowChecker::checkMulOverflow(llvm::BinaryOperator& I, MiriEmulator& emulator) {
    if (!emulator.hasExecutionContext() || !I.getType()->isIntegerTy()) {
        return;
    }
    
    auto& ec = emulator.getCurrentExecutionContext();
    auto& globalEc = emulator.getGlobalExecutionContext();
    
    // Get operands
    llvm::GenericValue op1Val = globalEc.getOperandValue(I.getOperand(0), ec);
    llvm::GenericValue op2Val = globalEc.getOperandValue(I.getOperand(1), ec);
    
    unsigned bits = I.getType()->getIntegerBitWidth();
    int64_t a = getSignedValue(op1Val, bits);
    int64_t b = getSignedValue(op2Val, bits);
    
    if (willSignedMulOverflow(a, b, bits)) {
        BugContext ctx;
        ctx.addValue("operand1", static_cast<uint64_t>(a));
        ctx.addValue("operand2", static_cast<uint64_t>(b));
        
        std::ostringstream oss;
        oss << "Signed integer overflow in multiplication: " << a << " * " << b
            << " (bit width=" << bits << ")";
        
        emulator.reportBug(BugType::SignedIntegerOverflow, &I, oss.str(), ctx);
    }
}

void IntegerOverflowChecker::checkNegOverflow(llvm::BinaryOperator& I, MiriEmulator& emulator) {
    // Negation is implemented as 0 - x, so it's caught by checkSubOverflow
}

bool IntegerOverflowChecker::willSignedAddOverflow(int64_t a, int64_t b, unsigned bits) {
    if (bits >= 64) {
        // For 64-bit, use builtin overflow detection
        int64_t result;
        return __builtin_saddll_overflow(a, b, &result);
    }
    
    // For smaller bit widths, compute the max/min values
    int64_t max_val = (1LL << (bits - 1)) - 1;
    int64_t min_val = -(1LL << (bits - 1));
    
    // Check for overflow
    if (b > 0 && a > max_val - b) {
        return true;  // Positive overflow
    }
    if (b < 0 && a < min_val - b) {
        return true;  // Negative overflow (underflow)
    }
    
    return false;
}

bool IntegerOverflowChecker::willSignedSubOverflow(int64_t a, int64_t b, unsigned bits) {
    if (bits >= 64) {
        int64_t result;
        return __builtin_ssubll_overflow(a, b, &result);
    }
    
    int64_t max_val = (1LL << (bits - 1)) - 1;
    int64_t min_val = -(1LL << (bits - 1));
    
    // Check for overflow: a - b where b is negative can overflow
    if (b < 0 && a > max_val + b) {
        return true;
    }
    // Check for underflow: a - b where b is positive can underflow
    if (b > 0 && a < min_val + b) {
        return true;
    }
    
    return false;
}

bool IntegerOverflowChecker::willSignedMulOverflow(int64_t a, int64_t b, unsigned bits) {
    if (bits >= 64) {
        int64_t result;
        return __builtin_smulll_overflow(a, b, &result);
    }
    
    int64_t max_val = (1LL << (bits - 1)) - 1;
    int64_t min_val = -(1LL << (bits - 1));
    
    // Special cases
    if (a == 0 || b == 0) {
        return false;
    }
    
    // Check for overflow
    if (a > 0) {
        if (b > 0) {
            // Both positive
            return a > max_val / b;
        } else {
            // a positive, b negative
            return b < min_val / a;
        }
    } else {
        if (b > 0) {
            // a negative, b positive
            return a < min_val / b;
        } else {
            // Both negative
            return a < max_val / b;
        }
    }
}

int64_t IntegerOverflowChecker::getSignedValue(const llvm::GenericValue& val, unsigned bits) {
    if (bits > 64) {
        bits = 64;
    }
    
    // Get the raw value
    uint64_t raw = val.IntVal.getZExtValue();
    
    // Sign extend if necessary
    if (bits < 64) {
        uint64_t sign_bit = 1ULL << (bits - 1);
        if (raw & sign_bit) {
            // Negative number - sign extend
            uint64_t mask = ~((1ULL << bits) - 1);
            raw |= mask;
        }
    }
    
    return static_cast<int64_t>(raw);
}

} // namespace miri

