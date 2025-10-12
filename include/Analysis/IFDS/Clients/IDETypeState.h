#pragma once

#include <Analysis/IFDS/IFDSFramework.h>
#include <llvm/IR/Instruction.h>
#include <string>
#include <unordered_map>

namespace ifds {

// Simple typestate lattice
enum class TypeState { Top, A, B, C, Error, Bottom };

class IDETypeState : public IDEProblem<const llvm::Value*, TypeState> {
public:
    using Fact = const llvm::Value*;
    using Value = TypeState;

    // IFDS interface
    Fact zero_fact() const override { return nullptr; }
    FactSet normal_flow(const llvm::Instruction* stmt, const Fact& fact) override;
    FactSet call_flow(const llvm::CallInst* call, const llvm::Function* callee, const Fact& fact) override;
    FactSet return_flow(const llvm::CallInst* call, const llvm::Function* callee, const Fact& exit_fact, const Fact& call_fact) override;
    FactSet call_to_return_flow(const llvm::CallInst* call, const Fact& fact) override;
    FactSet initial_facts(const llvm::Function* main) override;

    // Value domain
    Value top_value() const override { return Value::Top; }
    Value bottom_value() const override { return Value::Bottom; }
    Value join(const Value& v1, const Value& v2) const override;

    // Edge functions
    EdgeFunction normal_edge_function(const llvm::Instruction* stmt, const Fact& src_fact, const Fact& tgt_fact) override;
    EdgeFunction call_edge_function(const llvm::CallInst* call, const Fact& src_fact, const Fact& tgt_fact) override;
    EdgeFunction return_edge_function(const llvm::CallInst* call, const Fact& exit_fact, const Fact& ret_fact) override;
    EdgeFunction call_to_return_edge_function(const llvm::CallInst* call, const Fact& src_fact, const Fact& tgt_fact) override;

    // Configuration: map from callee name or instruction opcode to transitions
    void setTransition(const std::string& key, Value from, Value to) { transitions[key][from] = to; }

private:
    std::unordered_map<std::string, std::unordered_map<Value, Value>> transitions;
};

} // namespace ifds
