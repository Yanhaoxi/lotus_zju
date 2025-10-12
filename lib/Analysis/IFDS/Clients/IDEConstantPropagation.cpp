#include <Analysis/IFDS/Clients/IDEConstantPropagation.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Operator.h>

namespace ifds {

IDEConstantPropagation::FactSet IDEConstantPropagation::initial_facts(const llvm::Function* main) {
    FactSet seeds;
    for (const llvm::Argument& arg : main->args()) {
        seeds.insert(&arg);
    }
    return seeds;
}

IDEConstantPropagation::Value IDEConstantPropagation::join(const Value& v1, const Value& v2) const {
    if (v1.kind == LCPValue::Bottom) return v2;
    if (v2.kind == LCPValue::Bottom) return v1;
    if (v1.kind == LCPValue::Top || v2.kind == LCPValue::Top) return Value::top();
    return (v1.value == v2.value) ? v1 : Value::top();
}

bool IDEConstantPropagation::definesValue(const llvm::Instruction* I) {
    return !I->getType()->isVoidTy();
}

const llvm::Value* IDEConstantPropagation::getDefinedValue(const llvm::Instruction* I) {
    return definesValue(I) ? static_cast<const llvm::Value*>(I) : nullptr;
}

bool IDEConstantPropagation::isCopy(const llvm::Instruction* I, const llvm::Value*& from) {
    if (auto* store = llvm::dyn_cast<llvm::StoreInst>(I)) {
        from = store->getValueOperand();
        return true;
    }
    if (auto* bitcastInst = llvm::dyn_cast<llvm::BitCastInst>(I)) {
        from = bitcastInst->getOperand(0);
        return true;
    }
    if (auto* move = llvm::dyn_cast<llvm::UnaryOperator>(I)) {
        if (move->getOpcode() == llvm::Instruction::FNeg) return false;
        from = move->getOperand(0);
        return true;
    }
    return false;
}

llvm::Optional<long long> IDEConstantPropagation::asConst(const llvm::Value* v) {
    if (auto* ci = llvm::dyn_cast<llvm::ConstantInt>(v)) {
        return static_cast<long long>(ci->getSExtValue());
    }
    return llvm::None;
}

llvm::Optional<long long> IDEConstantPropagation::applyBinOp(unsigned opcode, long long a, long long b) {
    switch (opcode) {
        case llvm::Instruction::Add: return a + b;
        case llvm::Instruction::Sub: return a - b;
        case llvm::Instruction::Mul: return a * b;
        case llvm::Instruction::SDiv: return b != 0 ? llvm::Optional<long long>(a / b) : llvm::None;
        case llvm::Instruction::SRem: return b != 0 ? llvm::Optional<long long>(a % b) : llvm::None;
        default: return llvm::None;
    }
}

IDEConstantPropagation::FactSet IDEConstantPropagation::normal_flow(const llvm::Instruction* stmt, const Fact& fact) {
    FactSet out;
    // propagate same fact by default
    if (fact) out.insert(fact);
    if (const llvm::Value* def = getDefinedValue(stmt)) {
        out.insert(def);
    }
    return out;
}

IDEConstantPropagation::FactSet IDEConstantPropagation::call_flow(const llvm::CallInst* call, const llvm::Function* callee, const Fact& fact) {
    FactSet out;
    if (!callee || callee->isDeclaration()) return out;
    // map actual to formal by position
    for (unsigned i = 0; i < call->arg_size() && i < callee->arg_size(); ++i) {
        if (fact == call->getArgOperand(i)) {
            auto it = callee->arg_begin();
            std::advance(it, i);
            out.insert(&*it);
        }
    }
    return out;
}

IDEConstantPropagation::FactSet IDEConstantPropagation::return_flow(const llvm::CallInst* call, const llvm::Function* callee, const Fact& /*exit_fact*/, const Fact& call_fact) {
    FactSet out;
    if (!callee || callee->isDeclaration()) return out;
    // propagate caller facts unchanged
    if (call_fact) out.insert(call_fact);
    // also link callee return back to call result if applicable
    if (!call->getType()->isVoidTy()) {
        if (const llvm::Value* callDef = static_cast<const llvm::Value*>(call)) {
            out.insert(callDef);
        }
    }
    return out;
}

IDEConstantPropagation::FactSet IDEConstantPropagation::call_to_return_flow(const llvm::CallInst* call, const Fact& fact) {
    FactSet out;
    // conservative: propagate caller facts across unknown call
    if (fact) out.insert(fact);
    if (!call->getType()->isVoidTy()) {
        out.insert(static_cast<const llvm::Value*>(call));
    }
    return out;
}

IDEConstantPropagation::EdgeFunction IDEConstantPropagation::normal_edge_function(const llvm::Instruction* stmt, const Fact& src_fact, const Fact& tgt_fact) {
    // If this instruction defines tgt_fact via constant or binop, compute value
    if (const llvm::Value* def = getDefinedValue(stmt)) {
        if (tgt_fact == def) {
            // constant assignment
            auto c = asConst(def);
            if (c.hasValue()) {
                long long k = c.getValue();
                return [k](const Value& v) {
                    (void)v; return Value::constant(k);
                };
            }
            // copy
            const llvm::Value* from = nullptr;
            if (isCopy(stmt, from) && from == src_fact) {
                return [](const Value& v) { return v; };
            }
            // binary op
            if (auto* bin = llvm::dyn_cast<llvm::BinaryOperator>(stmt)) {
                const llvm::Value* op0 = bin->getOperand(0);
                const llvm::Value* op1 = bin->getOperand(1);
                unsigned opc = bin->getOpcode();
                return [opc, op0, op1, src_fact](const Value& v) {
                    // v carries the value for src_fact; only compute when both operands determined
                    if (src_fact != op0 && src_fact != op1) return LCPValue::bottom();
                    auto c0 = asConst(op0);
                    auto c1 = asConst(op1);
                    if (c0.hasValue() && c1.hasValue()) {
                        auto r = applyBinOp(opc, c0.getValue(), c1.getValue());
                        if (r.hasValue()) return LCPValue::constant(r.getValue());
                        return LCPValue::top();
                    }
                    // if one is variable, rely on composed flows to bring the other
                    return v;
                };
            }
            // otherwise, unknown definition => top
            return [](const Value& /*v*/) { return LCPValue::top(); };
        }
    }
    // default: identity
    return [](const Value& v) { return v; };
}

IDEConstantPropagation::EdgeFunction IDEConstantPropagation::call_edge_function(const llvm::CallInst* call, const Fact& src_fact, const Fact& tgt_fact) {
    // actual to formal: identity on carried constant
    (void)call; (void)src_fact; (void)tgt_fact;
    return [](const Value& v) { return v; };
}

IDEConstantPropagation::EdgeFunction IDEConstantPropagation::return_edge_function(const llvm::CallInst* call, const Fact& /*exit_fact*/, const Fact& ret_fact) {
    // callee return to caller result
    if (!call->getType()->isVoidTy()) {
        const llvm::Value* callDef = static_cast<const llvm::Value*>(call);
        if (ret_fact == callDef) {
            // propagate value from exit_fact (assumed return value) directly
            return [](const Value& v) { return v; };
        }
    }
    return [](const Value& v) { return v; };
}

IDEConstantPropagation::EdgeFunction IDEConstantPropagation::call_to_return_edge_function(const llvm::CallInst* call, const Fact& src_fact, const Fact& tgt_fact) {
    // unknown function: kill definition of call result => top; keep others
    if (!call->getType()->isVoidTy()) {
        const llvm::Value* callDef = static_cast<const llvm::Value*>(call);
        if (tgt_fact == callDef) {
            return [](const Value& /*v*/) { return LCPValue::top(); };
        }
    }
    (void)src_fact; (void)tgt_fact;
    return [](const Value& v) { return v; };
}

} // namespace ifds
