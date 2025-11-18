/**
 * @file InvalidShiftChecker.cpp
 * @brief Implementation of invalid shift checker
 */

#include "Analysis/llvmir-emul/BugCheckers.h"
#include "Analysis/llvmir-emul/MiriEmulator.h"
#include <llvm/IR/Instructions.h>
#include <sstream>

namespace miri {

void InvalidShiftChecker::preVisit(llvm::Instruction& I, MiriEmulator& emulator) {
    if (auto* BO = llvm::dyn_cast<llvm::BinaryOperator>(&I)) {
        unsigned opcode = BO->getOpcode();
        if (opcode == llvm::Instruction::Shl ||
            opcode == llvm::Instruction::LShr ||
            opcode == llvm::Instruction::AShr) {
            checkShift(*BO, emulator);
        }
    }
}

void InvalidShiftChecker::checkShift(llvm::BinaryOperator& I, MiriEmulator& emulator) {
    if (!emulator.hasExecutionContext() || !I.getType()->isIntegerTy()) {
        return;
    }
    
    auto& ec = emulator.getCurrentExecutionContext();
    auto& globalEc = emulator.getGlobalExecutionContext();
    
    // Get shift amount (second operand)
    llvm::Value* shiftOp = I.getOperand(1);
    llvm::GenericValue shiftVal = globalEc.getOperandValue(shiftOp, ec);
    
    uint64_t shiftAmount = getShiftAmount(shiftVal, I.getType());
    unsigned bitWidth = I.getType()->getIntegerBitWidth();
    
    // Check if shift amount is invalid
    bool isInvalid = false;
    std::string reason;
    
    if (shiftAmount >= bitWidth) {
        isInvalid = true;
        std::ostringstream oss;
        oss << "Shift amount (" << shiftAmount << ") >= bit width (" << bitWidth << ")";
        reason = oss.str();
    }
    
    // For signed shifts with negative amounts (if represented as large unsigned)
    // This is caught by the >= bitWidth check in most cases
    
    if (isInvalid) {
        BugContext ctx;
        ctx.addValue("shift_amount", shiftAmount);
        ctx.addValue("bit_width", bitWidth);
        
        // Get value being shifted for context
        llvm::Value* valueOp = I.getOperand(0);
        llvm::GenericValue valueVal = globalEc.getOperandValue(valueOp, ec);
        uint64_t value = valueVal.IntVal.getZExtValue();
        ctx.addValue("value", value);
        
        std::ostringstream oss;
        oss << "Invalid shift operation: " << reason;
        
        const char* op_name = "shift";
        switch (I.getOpcode()) {
            case llvm::Instruction::Shl: op_name = "left shift"; break;
            case llvm::Instruction::LShr: op_name = "logical right shift"; break;
            case llvm::Instruction::AShr: op_name = "arithmetic right shift"; break;
        }
        
        ctx.additional_info = std::string("Operation: ") + op_name;
        
        emulator.reportBug(BugType::InvalidShift, &I, oss.str(), ctx);
    }
}

uint64_t InvalidShiftChecker::getShiftAmount(const llvm::GenericValue& val, llvm::Type* type) {
    return val.IntVal.getZExtValue();
}

} // namespace miri

