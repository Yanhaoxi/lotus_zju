#include <Analysis/IFDS/Clients/IDETypeState.h>
#include <llvm/IR/Instructions.h>

namespace ifds {

IDETypeState::FactSet IDETypeState::initial_facts(const llvm::Function* main) {
    FactSet seeds;
    for (const llvm::Argument& arg : main->args()) {
        seeds.insert(&arg);
    }
    return seeds;
}

IDETypeState::Value IDETypeState::join(const Value& v1, const Value& v2) const {
    if (v1 == Value::Bottom) return v2;
    if (v2 == Value::Bottom) return v1;
    if (v1 == v2) return v1;
    if (v1 == Value::Top || v2 == Value::Top) return Value::Top;
    // conflicting states collapse to Top (unknown)
    return Value::Top;
}

IDETypeState::FactSet IDETypeState::normal_flow(const llvm::Instruction* stmt, const Fact& fact) {
    FactSet out;
    if (fact) out.insert(fact);
    if (!stmt->getType()->isVoidTy()) out.insert(static_cast<const llvm::Value*>(stmt));
    return out;
}

IDETypeState::FactSet IDETypeState::call_flow(const llvm::CallInst* call, const llvm::Function* callee, const Fact& fact) {
    FactSet out;
    if (!callee || callee->isDeclaration()) return out;
    // map actual to formal
    for (unsigned i = 0; i < call->arg_size() && i < callee->arg_size(); ++i) {
        if (fact == call->getArgOperand(i)) {
            auto it = callee->arg_begin();
            std::advance(it, i);
            out.insert(&*it);
        }
    }
    return out;
}

IDETypeState::FactSet IDETypeState::return_flow(const llvm::CallInst* call, const llvm::Function* callee, const Fact& exit_fact, const Fact& call_fact) {
    (void)exit_fact; (void)callee;
    FactSet out;
    if (call_fact) out.insert(call_fact);
    if (!call->getType()->isVoidTy()) out.insert(static_cast<const llvm::Value*>(call));
    return out;
}

IDETypeState::FactSet IDETypeState::call_to_return_flow(const llvm::CallInst* call, const Fact& fact) {
    FactSet out;
    if (fact) out.insert(fact);
    if (!call->getType()->isVoidTy()) out.insert(static_cast<const llvm::Value*>(call));
    return out;
}

IDETypeState::EdgeFunction IDETypeState::normal_edge_function(const llvm::Instruction* stmt, const Fact& src_fact, const Fact& tgt_fact) {
    (void)src_fact; (void)tgt_fact;
    // default: identity
    // transitions may be keyed by opcode name like "store", etc.
    std::string key;
    if (auto* inst = llvm::dyn_cast<llvm::Instruction>(stmt)) {
        key = inst->getOpcodeName();
    }
    auto itK = transitions.find(key);
    if (itK == transitions.end()) return [](const Value& v) { return v; };

    return [mapping = itK->second](const Value& v) {
        auto it = mapping.find(v);
        if (it != mapping.end()) return it->second;
        return v;
    };
}

IDETypeState::EdgeFunction IDETypeState::call_edge_function(const llvm::CallInst* call, const Fact& src_fact, const Fact& tgt_fact) {
    (void)src_fact; (void)tgt_fact;
    std::string key = call->getCalledFunction() ? call->getCalledFunction()->getName().str() : std::string();
    auto itK = transitions.find(key);
    if (itK == transitions.end()) return [](const Value& v) { return v; };
    return [mapping = itK->second](const Value& v) {
        auto it = mapping.find(v);
        if (it != mapping.end()) return it->second;
        return v;
    };
}

IDETypeState::EdgeFunction IDETypeState::return_edge_function(const llvm::CallInst* call, const Fact& exit_fact, const Fact& ret_fact) {
    (void)call; (void)exit_fact; (void)ret_fact;
    return [](const Value& v) { return v; };
}

IDETypeState::EdgeFunction IDETypeState::call_to_return_edge_function(const llvm::CallInst* call, const Fact& src_fact, const Fact& tgt_fact) {
    (void)src_fact; (void)tgt_fact; (void)call;
    return [](const Value& v) { return v; };
}

} // namespace ifds
