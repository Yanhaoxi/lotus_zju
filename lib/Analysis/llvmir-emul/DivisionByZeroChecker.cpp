/**
 * @file DivisionByZeroChecker.cpp
 * @brief Implementation of division by zero checker
 */

#include "Analysis/llvmir-emul/BugCheckers.h"
#include "Analysis/llvmir-emul/MiriEmulator.h"
#include <llvm/IR/Instructions.h>
#include <sstream>

namespace miri {

void DivisionByZeroChecker::preVisit(llvm::Instruction& I, MiriEmulator& emulator) {
    if (auto* BO = llvm::dyn_cast<llvm::BinaryOperator>(&I)) {
        unsigned opcode = BO->getOpcode();
        if (opcode == llvm::Instruction::UDiv ||
            opcode == llvm::Instruction::SDiv ||
            opcode == llvm::Instruction::URem ||
            opcode == llvm::Instruction::SRem ||
            opcode == llvm::Instruction::FDiv ||
            opcode == llvm::Instruction::FRem) {
            checkDivision(*BO, emulator);
        }
    }
}

void DivisionByZeroChecker::checkDivision(llvm::BinaryOperator& I, MiriEmulator& emulator) {
    if (!emulator.hasExecutionContext()) {
        return;
    }
    
    auto& ec = emulator.getCurrentExecutionContext();
    auto& globalEc = emulator.getGlobalExecutionContext();
    
    // Get divisor (second operand)
    llvm::Value* divisor = I.getOperand(1);
    llvm::GenericValue divisorVal = globalEc.getOperandValue(divisor, ec);
    
    // Check if divisor is zero
    if (isZero(divisorVal, I.getType())) {
        BugContext ctx;
        ctx.addValue("divisor", 0);
        
        // Get dividend for context
        llvm::Value* dividend = I.getOperand(0);
        llvm::GenericValue dividendVal = globalEc.getOperandValue(dividend, ec);
        
        // Add dividend value (for integer types)
        if (I.getType()->isIntegerTy()) {
            uint64_t divVal = dividendVal.IntVal.getZExtValue();
            ctx.addValue("dividend", divVal);
        }
        
        std::ostringstream oss;
        oss << "Division by zero: ";
        I.print(llvm::errs());
        
        emulator.reportBug(BugType::DivisionByZero, &I, oss.str(), ctx);
    }
}

bool DivisionByZeroChecker::isZero(const llvm::GenericValue& val, llvm::Type* type) {
    if (type->isIntegerTy()) {
        return val.IntVal == 0;
    } else if (type->isFloatTy()) {
        return val.FloatVal == 0.0f;
    } else if (type->isDoubleTy()) {
        return val.DoubleVal == 0.0;
    }
    return false;
}

} // namespace miri

