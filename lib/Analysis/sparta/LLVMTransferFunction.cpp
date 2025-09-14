/*
 * LLVM Transfer Function Implementation
 * Instruction transfer functions for LLVM abstract interpreter
 */

#include <Analysis/sparta/LLVMAbstractInterpreter.h>

#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/CFG.h>

#include <string>

namespace sparta {
namespace llvm_ai {

// ============================================================================
// LLVMTransferFunction Implementation
// ============================================================================

void LLVMTransferFunction::apply_instruction(const llvm::Instruction* inst, LLVMAbstractState& state) {
    if (state.is_bottom()) return;
    
    switch (inst->getOpcode()) {
        case llvm::Instruction::Add:
        case llvm::Instruction::Sub:
        case llvm::Instruction::Mul:
        case llvm::Instruction::SDiv:
        case llvm::Instruction::UDiv:
            handle_binary_operator(llvm::cast<llvm::BinaryOperator>(inst), state);
            break;
        case llvm::Instruction::ICmp:
            handle_icmp(llvm::cast<llvm::ICmpInst>(inst), state);
            break;
        case llvm::Instruction::Load:
            handle_load(llvm::cast<llvm::LoadInst>(inst), state);
            break;
        case llvm::Instruction::Store:
            handle_store(llvm::cast<llvm::StoreInst>(inst), state);
            break;
        case llvm::Instruction::Alloca:
            handle_alloca(llvm::cast<llvm::AllocaInst>(inst), state);
            break;
        case llvm::Instruction::Call:
            handle_call(llvm::cast<llvm::CallInst>(inst), state);
            break;
        case llvm::Instruction::PHI:
            handle_phi(llvm::cast<llvm::PHINode>(inst), state);
            break;
        case llvm::Instruction::ZExt:
        case llvm::Instruction::SExt:
        case llvm::Instruction::Trunc:
        case llvm::Instruction::BitCast:
            handle_cast(llvm::cast<llvm::CastInst>(inst), state);
            break;
        case llvm::Instruction::GetElementPtr:
            handle_gep(llvm::cast<llvm::GetElementPtrInst>(inst), state);
            break;
        default:
            // For unhandled instructions, set result to top
            if (!inst->getType()->isVoidTy()) {
                state.set_value(inst, LLVMValueDomain::top());
            }
            break;
    }
}

void LLVMTransferFunction::handle_binary_operator(const llvm::BinaryOperator* binop, LLVMAbstractState& state) {
    LLVMValueDomain lhs = state.get_value(binop->getOperand(0));
    LLVMValueDomain rhs = state.get_value(binop->getOperand(1));
    
    LLVMValueDomain result;
    
    switch (binop->getOpcode()) {
        case llvm::Instruction::Add:
            result = lhs.add(rhs);
            break;
        case llvm::Instruction::Sub:
            result = lhs.sub(rhs);
            break;
        case llvm::Instruction::Mul:
            result = lhs.mul(rhs);
            break;
        case llvm::Instruction::SDiv:
        case llvm::Instruction::UDiv:
            result = lhs.div(rhs);
            break;
        default:
            result = LLVMValueDomain::top();
            break;
    }
    
    state.set_value(binop, result);
}

void LLVMTransferFunction::handle_icmp(const llvm::ICmpInst* icmp, LLVMAbstractState& state) {
    LLVMValueDomain lhs = state.get_value(icmp->getOperand(0));
    LLVMValueDomain rhs = state.get_value(icmp->getOperand(1));
    
    LLVMValueDomain result;
    
    switch (icmp->getPredicate()) {
        case llvm::ICmpInst::ICMP_EQ:
            result = lhs.icmp_eq(rhs);
            break;
        case llvm::ICmpInst::ICMP_NE:
            result = lhs.icmp_ne(rhs);
            break;
        case llvm::ICmpInst::ICMP_SLT:
            result = lhs.icmp_slt(rhs);
            break;
        case llvm::ICmpInst::ICMP_SLE:
            result = lhs.icmp_sle(rhs);
            break;
        case llvm::ICmpInst::ICMP_SGT:
            result = rhs.icmp_slt(lhs);
            break;
        case llvm::ICmpInst::ICMP_SGE:
            result = rhs.icmp_sle(lhs);
            break;
        default:
            result = LLVMValueDomain(0, 1);
            break;
    }
    
    state.set_value(icmp, result);
}

void LLVMTransferFunction::handle_load(const llvm::LoadInst* load, LLVMAbstractState& state) {
    const llvm::Value* ptr = load->getPointerOperand();
    LLVMValueDomain loaded_value = state.load_memory(ptr);
    state.set_value(load, loaded_value);
}

void LLVMTransferFunction::handle_store(const llvm::StoreInst* store, LLVMAbstractState& state) {
    const llvm::Value* value = store->getValueOperand();
    const llvm::Value* ptr = store->getPointerOperand();
    
    LLVMValueDomain stored_value = state.get_value(value);
    state.store_memory(ptr, stored_value);
}

void LLVMTransferFunction::handle_alloca(const llvm::AllocaInst* alloca, LLVMAbstractState& state) {
    // Alloca creates a new memory location
    state.set_value(alloca, LLVMValueDomain(alloca));
}

void LLVMTransferFunction::handle_call(const llvm::CallInst* call, LLVMAbstractState& state) {
    // Enhanced call handling with interprocedural analysis support
    const llvm::Function* callee = call->getCalledFunction();
    
    if (!callee || callee->isDeclaration()) {
        // External function call - use conservative approximation
        if (!call->getType()->isVoidTy()) {
            state.set_value(call, LLVMValueDomain::top());
        }
        
        // Model side effects of common library functions
        handle_library_function_call(call, state);
        return;
    }
    
    // Prepare arguments for interprocedural analysis
    std::vector<LLVMValueDomain> arg_values;
    for (unsigned i = 0; i < call->getNumOperands() - 1; ++i) {
        const llvm::Value* arg = call->getOperand(i);
        arg_values.push_back(state.get_value(arg));
    }
    
    // For now, use a simplified approach - in full implementation,
    // this would trigger interprocedural analysis
    if (!call->getType()->isVoidTy()) {
        // Use function summary if available, otherwise top
        state.set_value(call, LLVMValueDomain::top());
    }
    
    // Model memory effects of the call
    model_call_side_effects(call, state);
}

void LLVMTransferFunction::handle_phi(const llvm::PHINode* phi, LLVMAbstractState& state) {
    // PHI nodes are handled by the fixpoint iterator through edge analysis
    // Here we just set to top as a fallback
    state.set_value(phi, LLVMValueDomain::top());
}

void LLVMTransferFunction::handle_cast(const llvm::CastInst* cast, LLVMAbstractState& state) {
    LLVMValueDomain operand = state.get_value(cast->getOperand(0));
    
    // For integer casts, we preserve the value if possible
    if (cast->getDestTy()->isIntegerTy() && operand.is_constant()) {
        state.set_value(cast, operand);
    } else {
        state.set_value(cast, LLVMValueDomain::top());
    }
}

void LLVMTransferFunction::handle_gep(const llvm::GetElementPtrInst* gep, LLVMAbstractState& state) {
    // Simplified GEP handling - just propagate the base pointer
    LLVMValueDomain base = state.get_value(gep->getPointerOperand());
    
    if (base.is_pointer()) {
        state.set_value(gep, base);
    } else {
        state.set_value(gep, LLVMValueDomain::top());
    }
}

// ============================================================================
// Branch Condition Analysis
// ============================================================================

std::pair<LLVMAbstractState, LLVMAbstractState> 
LLVMTransferFunction::analyze_branch_condition(const llvm::BranchInst* br, const LLVMAbstractState& state) {
    if (br->isUnconditional()) {
        return {state, LLVMAbstractState::bottom()};
    }
    
    LLVMValueDomain condition = state.get_value(br->getCondition());
    
    LLVMAbstractState true_state = state;
    LLVMAbstractState false_state = state;
    
    if (condition.is_constant()) {
        auto constant = condition.get_constant();
        if (constant && *constant != 0) {
            false_state.set_to_bottom();
        } else {
            true_state.set_to_bottom();
        }
    }
    
    return {true_state, false_state};
}

// ============================================================================
// Library Function Handling
// ============================================================================

void LLVMTransferFunction::handle_library_function_call(const llvm::CallInst* call, LLVMAbstractState& state) {
    // Model common library function side effects
    const llvm::Function* callee = call->getCalledFunction();
    if (!callee) return;
    
    std::string func_name = callee->getName().str();
    
    // Memory allocation functions
    if (func_name == "malloc" || func_name == "calloc" || func_name == "realloc") {
        if (!call->getType()->isVoidTy()) {
            // Return a new abstract memory location
            state.set_value(call, LLVMValueDomain(call));
        }
    }
    // Memory deallocation
    else if (func_name == "free") {
        // Invalidate the freed memory location
        if (call->getNumOperands() > 1) {
            const llvm::Value* ptr_arg = call->getOperand(0);
            state.invalidate_memory(ptr_arg);
        }
    }
    // String/memory functions
    else if (func_name == "memcpy" || func_name == "memmove" || func_name == "strcpy") {
        if (call->getNumOperands() >= 3) {
            const llvm::Value* dest = call->getOperand(0);
            const llvm::Value* src = call->getOperand(1);
            LLVMValueDomain src_value = state.load_memory(src);
            state.store_memory(dest, src_value);
        }
    }
    // Functions that don't modify memory but may return values
    else if (func_name == "strlen" || func_name == "strcmp") {
        if (!call->getType()->isVoidTy()) {
            // Return unknown integer value
            state.set_value(call, LLVMValueDomain::top());
        }
    }
}

void LLVMTransferFunction::model_call_side_effects(const llvm::CallInst* call, LLVMAbstractState& state) {
    // Model potential side effects of function calls
    const llvm::Function* callee = call->getCalledFunction();
    if (!callee) return;
    
    // Conservative assumption: function may modify any memory reachable through pointers
    for (unsigned i = 0; i < call->getNumOperands() - 1; ++i) {
        const llvm::Value* arg = call->getOperand(i);
        if (arg->getType()->isPointerTy()) {
            // Assume the function may modify memory pointed to by this argument
            state.store_memory(arg, LLVMValueDomain::top());
        }
    }
}

} // namespace llvm_ai
} // namespace sparta
