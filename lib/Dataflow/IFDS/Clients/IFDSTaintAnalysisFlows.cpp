/*
 * Taint Analysis Implementation - Flow Functions
 * 
 * Author: rainoftime
 */

#include "Annotation/Taint/TaintConfigManager.h"
#include "Dataflow/IFDS/Clients/IFDSTaintAnalysis.h"
#include "Utils/LLVM/Demangle.h"

#include <llvm/Analysis/ValueTracking.h>
#include <llvm/Support/raw_ostream.h>

#include <algorithm>
#include <iterator>
#include <set>
#include <unordered_set>

namespace ifds {

namespace {
std::string strip_signature(const std::string& demangled) {
    auto paren_pos = demangled.find('(');
    if (paren_pos == std::string::npos) {
        return demangled;
    }
    return demangled.substr(0, paren_pos);
}
} // namespace

// ============================================================================
// TaintAnalysis Implementation (Flow)
// ============================================================================

TaintAnalysis::TaintAnalysis() {
    if (!taint_config::load_default_config()) {
        llvm::errs() << "Error: Could not load taint configuration\n";
        return;
    }
    
    auto& config = TaintConfigManager::getInstance();
    auto sources = config.get_all_source_functions();
    auto sinks = config.get_all_sink_functions();
    
    m_source_functions.insert(sources.begin(), sources.end());
    m_sink_functions.insert(sinks.begin(), sinks.end());
    
    llvm::outs() << "Loaded " << sources.size() << " sources and " << sinks.size() << " sinks from configuration\n";
}

bool TaintAnalysis::taint_may_alias(const llvm::Value* v1, const llvm::Value* v2) const {
    if (may_alias(v1, v2)) {
        return true;
    }
    if (!v1 || !v2) {
        return false;
    }
    const llvm::Value* base1 = llvm::getUnderlyingObject(v1);
    const llvm::Value* base2 = llvm::getUnderlyingObject(v2);
    return base1 && base2 && base1 == base2;
}

TaintFact TaintAnalysis::zero_fact() const {
    return TaintFact::zero();
}

TaintAnalysis::FactSet TaintAnalysis::normal_flow(const llvm::Instruction* stmt, const TaintFact& fact) {
    FactSet result;
    
    // Always propagate zero fact
    if (fact.is_zero()) {
        result.insert(fact);
        return result;
    }
    
    // Helper to propagate existing facts
    auto propagate_fact = [&result, &fact]() { result.insert(fact); };
    
    if (auto* store = llvm::dyn_cast<llvm::StoreInst>(stmt)) {
        const llvm::Value* value = store->getValueOperand();
        const llvm::Value* ptr = store->getPointerOperand();
        
        if (fact.is_tainted_var() && fact.get_value() == value) {
            result.insert(TaintFact::tainted_memory(ptr, fact.get_source()));
        }
        
        propagate_fact();
        
    } else if (auto* load = llvm::dyn_cast<llvm::LoadInst>(stmt)) {
        const llvm::Value* ptr = load->getPointerOperand();
        
        if ((fact.is_tainted_memory() && taint_may_alias(fact.get_memory_location(), ptr)) ||
            (fact.is_tainted_var() && fact.get_value() == ptr)) {
            result.insert(TaintFact::tainted_var(load, fact.get_source()));
        }
        
        propagate_fact();
        
    } else if (auto* binop = llvm::dyn_cast<llvm::BinaryOperator>(stmt)) {
        const llvm::Value* lhs = binop->getOperand(0);
        const llvm::Value* rhs = binop->getOperand(1);
        
        if (fact.is_tainted_var() && (fact.get_value() == lhs || fact.get_value() == rhs)) {
            result.insert(TaintFact::tainted_var(binop, fact.get_source()));
        }
        
        propagate_fact();
        
    } else if (auto* cmp = llvm::dyn_cast<llvm::CmpInst>(stmt)) {
        const llvm::Value* lhs = cmp->getOperand(0);
        const llvm::Value* rhs = cmp->getOperand(1);
        
        if (fact.is_tainted_var() && (fact.get_value() == lhs || fact.get_value() == rhs)) {
            result.insert(TaintFact::tainted_var(cmp, fact.get_source()));
        }
        
        propagate_fact();
        
    } else if (auto* select = llvm::dyn_cast<llvm::SelectInst>(stmt)) {
        const llvm::Value* cond = select->getCondition();
        const llvm::Value* true_val = select->getTrueValue();
        const llvm::Value* false_val = select->getFalseValue();
        
        if (fact.is_tainted_var() &&
            (fact.get_value() == cond || fact.get_value() == true_val || fact.get_value() == false_val)) {
            result.insert(TaintFact::tainted_var(select, fact.get_source()));
        }
        
        propagate_fact();
        
    } else if (auto* unary = llvm::dyn_cast<llvm::UnaryOperator>(stmt)) {
        const llvm::Value* operand = unary->getOperand(0);
        
        if (fact.is_tainted_var() && fact.get_value() == operand) {
            result.insert(TaintFact::tainted_var(unary, fact.get_source()));
        }
        
        propagate_fact();
        
    } else if (auto* cast = llvm::dyn_cast<llvm::CastInst>(stmt)) {
        if (fact.is_tainted_var() && fact.get_value() == cast->getOperand(0)) {
            result.insert(TaintFact::tainted_var(cast, fact.get_source()));
        }
        
        propagate_fact();
        
    } else if (auto* gep = llvm::dyn_cast<llvm::GetElementPtrInst>(stmt)) {
        if (fact.is_tainted_var() && fact.get_value() == gep->getPointerOperand()) {
            result.insert(TaintFact::tainted_var(gep, fact.get_source()));
        }

        if (fact.is_tainted_memory() &&
            taint_may_alias(fact.get_memory_location(), gep->getPointerOperand())) {
            result.insert(TaintFact::tainted_memory(gep, fact.get_source()));
        }
        
        propagate_fact();
        
    } else if (auto* phi = llvm::dyn_cast<llvm::PHINode>(stmt)) {
        if (fact.is_tainted_var()) {
            for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
                if (phi->getIncomingValue(i) == fact.get_value()) {
                    result.insert(TaintFact::tainted_var(phi, fact.get_source()));
                    break;
                }
            }
        }

        if (fact.is_tainted_memory() && phi->getType()->isPointerTy()) {
            for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
                const llvm::Value* incoming = phi->getIncomingValue(i);
                if (incoming && incoming->getType()->isPointerTy() &&
                    taint_may_alias(incoming, fact.get_memory_location())) {
                    result.insert(TaintFact::tainted_memory(phi, fact.get_source()));
                    break;
                }
            }
        }

        propagate_fact();
        
    } else if (auto* insert = llvm::dyn_cast<llvm::InsertValueInst>(stmt)) {
        const llvm::Value* agg = insert->getAggregateOperand();
        const llvm::Value* val = insert->getInsertedValueOperand();
        
        if (fact.is_tainted_var() && (fact.get_value() == agg || fact.get_value() == val)) {
            result.insert(TaintFact::tainted_var(insert, fact.get_source()));
        }
        
        propagate_fact();
        
    } else if (auto* extract = llvm::dyn_cast<llvm::ExtractValueInst>(stmt)) {
        const llvm::Value* agg = extract->getAggregateOperand();
        
        if (fact.is_tainted_var() && fact.get_value() == agg) {
            result.insert(TaintFact::tainted_var(extract, fact.get_source()));
        }
        
        propagate_fact();
        
    } else if (auto* insert_elem = llvm::dyn_cast<llvm::InsertElementInst>(stmt)) {
        const llvm::Value* vec = insert_elem->getOperand(0);
        const llvm::Value* val = insert_elem->getOperand(1);
        
        if (fact.is_tainted_var() && (fact.get_value() == vec || fact.get_value() == val)) {
            result.insert(TaintFact::tainted_var(insert_elem, fact.get_source()));
        }
        
        propagate_fact();
        
    } else if (auto* extract_elem = llvm::dyn_cast<llvm::ExtractElementInst>(stmt)) {
        const llvm::Value* vec = extract_elem->getVectorOperand();
        
        if (fact.is_tainted_var() && fact.get_value() == vec) {
            result.insert(TaintFact::tainted_var(extract_elem, fact.get_source()));
        }
        
        propagate_fact();
        
    } else if (auto* shuffle = llvm::dyn_cast<llvm::ShuffleVectorInst>(stmt)) {
        const llvm::Value* vec1 = shuffle->getOperand(0);
        const llvm::Value* vec2 = shuffle->getOperand(1);
        
        if (fact.is_tainted_var() && (fact.get_value() == vec1 || fact.get_value() == vec2)) {
            result.insert(TaintFact::tainted_var(shuffle, fact.get_source()));
        }
        
        propagate_fact();
        
    } else {
        propagate_fact();
    }
    
    return result;
}

TaintAnalysis::FactSet TaintAnalysis::call_flow(const llvm::CallInst* call, const llvm::Function* callee, 
                     const TaintFact& fact) {
    FactSet result;
    
    if (fact.is_zero()) {
        result.insert(fact);
        return result;
    }
    
    if (!callee || callee->isDeclaration()) {
        return result;
    }
    
    // Map caller facts to callee facts
    // Ensure we don't go beyond the number of actual function parameters
    unsigned num_args = std::min(call->arg_size(), (unsigned)std::distance(callee->arg_begin(), callee->arg_end()));
    
    for (unsigned i = 0; i < num_args; ++i) {
        const llvm::Value* arg = call->getArgOperand(i);
        if (!arg) continue;
        
        const auto *param_it = callee->arg_begin();
        std::advance(param_it, i);
        if (param_it == callee->arg_end()) break;
        
        if (fact.is_tainted_var()) {
            const llvm::Value* fact_val = fact.get_value();
            if (fact_val) {
                // Direct pointer comparison first, then alias check
                bool matches_arg = (arg == fact_val) ||
                    (fact_val->getType() && fact_val->getType()->isPointerTy() && taint_may_alias(arg, fact_val));
                if (matches_arg) {
                    result.insert(TaintFact::tainted_var(&*param_it, fact.get_source()));
                }
            }
        }
        
        if (fact.is_tainted_memory() && arg->getType() && arg->getType()->isPointerTy()) {
            const llvm::Value* fact_mem = fact.get_memory_location();
            if (fact_mem && fact_mem->getType() && fact_mem->getType()->isPointerTy() &&
                taint_may_alias(arg, fact_mem)) {
                result.insert(TaintFact::tainted_memory(&*param_it, fact.get_source()));
            }
        }
    }
    
    return result;
}

TaintAnalysis::FactSet TaintAnalysis::return_flow(const llvm::CallInst* call, const llvm::Function* callee,
                       const TaintFact& exit_fact, const TaintFact& call_fact) {
    FactSet result;
    
    if (exit_fact.is_zero()) {
        result.insert(exit_fact);
        return result;
    }
    
    // Map return values back to call site
    if (exit_fact.is_tainted_var()) {
        for (const llvm::BasicBlock& bb : *callee) {
            for (const llvm::Instruction& inst : bb) {
                if (auto* ret = llvm::dyn_cast<llvm::ReturnInst>(&inst)) {
                    if (ret->getReturnValue() == exit_fact.get_value()) {
                        result.insert(TaintFact::tainted_var(call, exit_fact.get_source()));
                    }
                }
            }
        }
    }

    // Map tainted memory on callee formals back to caller actuals.
    if (exit_fact.is_tainted_memory()) {
        unsigned num_args = std::min(call->arg_size(),
                                     (unsigned)std::distance(callee->arg_begin(), callee->arg_end()));
        for (unsigned i = 0; i < num_args; ++i) {
            const llvm::Value* actual = call->getArgOperand(i);
            if (!actual || !actual->getType() || !actual->getType()->isPointerTy()) continue;

            const auto *param_it = callee->arg_begin();
            std::advance(param_it, i);
            if (param_it == callee->arg_end()) break;

            if (taint_may_alias(&*param_it, exit_fact.get_memory_location())) {
                result.insert(TaintFact::tainted_memory(actual, exit_fact.get_source()));
            }
        }
    }
    
    if (!call_fact.is_zero()) {
        result.insert(call_fact);
    }
    
    return result;
}

TaintAnalysis::FactSet TaintAnalysis::call_to_return_flow(const llvm::CallInst* call, const TaintFact& fact) {
    FactSet result;

    const llvm::Function* callee = call->getCalledFunction();

    // Handle sources independently of incoming facts.
    if (is_source(call)) {
        if (!call->getType()->isVoidTy()) {
            result.insert(TaintFact::tainted_var(call, call));
            if (call->getType()->isPointerTy()) {
                result.insert(TaintFact::tainted_memory(call, call));
            }
        }
    }

    // Handle source function specs from config (may add additional facts).
    if (callee) {
        handle_source_function_specs(call, result);
    }

    // Always propagate zero fact
    if (fact.is_zero()) {
        result.insert(fact);
        return result;
    }

    if (!callee) {
        if (!kills_fact(call, fact)) {
            result.insert(fact);
        }
        return result;
    }

    // Handle PIPE specifications for taint propagation
    handle_pipe_specifications(call, fact, result);

    // Propagate facts that are not killed by the call
    if (!kills_fact(call, fact)) {
        result.insert(fact);
    }

    return result;
}

TaintAnalysis::FactSet TaintAnalysis::initial_facts(const llvm::Function* main) {
    FactSet result;
    result.insert(zero_fact());
    
    // Taint command line arguments
    for (const llvm::Argument& arg : main->args()) {
        if (arg.getType()->isPointerTy()) {
            result.insert(TaintFact::tainted_var(&arg));
        }
    }
    
    return result;
}

bool TaintAnalysis::is_source(const llvm::Instruction* inst) const {
    auto* call = llvm::dyn_cast<llvm::CallInst>(inst);
    if (!call || !call->getCalledFunction()) return false;
    
    auto raw_name = call->getCalledFunction()->getName().str();
    std::string func_name = taint_config::normalize_name(raw_name);
    if (m_source_functions.count(func_name) > 0) {
        return true;
    }

    // Fallback: demangle C++ names and match by suffix (e.g., "::source").
    std::string demangled_name = DemangleUtils::demangle(raw_name);
    std::string normalized_demangled = taint_config::normalize_name(strip_signature(demangled_name));
    for (const auto& source : m_source_functions) {
        if (normalized_demangled.size() >= source.size() &&
            normalized_demangled.compare(normalized_demangled.size() - source.size(),
                                         source.size(), source) == 0) {
            return true;
        }
    }

    return false;
}

bool TaintAnalysis::is_sink(const llvm::Instruction* inst) const {
    auto* call = llvm::dyn_cast<llvm::CallInst>(inst);
    if (!call || !call->getCalledFunction()) return false;
    
    auto raw_name = call->getCalledFunction()->getName().str();
    std::string func_name = taint_config::normalize_name(raw_name);
    if (m_sink_functions.count(func_name) > 0) {
        return true;
    }

    // Fallback: demangle C++ names and match by suffix (e.g., "::sink").
    std::string demangled_name = DemangleUtils::demangle(raw_name);
    std::string normalized_demangled = taint_config::normalize_name(strip_signature(demangled_name));
    for (const auto& sink : m_sink_functions) {
        if (normalized_demangled.size() >= sink.size() &&
            normalized_demangled.compare(normalized_demangled.size() - sink.size(),
                                         sink.size(), sink) == 0) {
            return true;
        }
    }

    return false;
}

void TaintAnalysis::add_source_function(const std::string& func_name) {
    m_source_functions.insert(func_name);
}

void TaintAnalysis::add_sink_function(const std::string& func_name) {
    m_sink_functions.insert(func_name);
}

bool TaintAnalysis::kills_fact(const llvm::CallInst* call, const TaintFact& fact) const {
    const llvm::Function* callee = call->getCalledFunction();
    if (!callee || !fact.is_tainted_var()) return false;
    
    static const std::unordered_set<std::string> sanitizers = {
        "strlen", "strcmp", "strncmp", "isdigit", "isalpha"
    };
    
    if (sanitizers.count(callee->getName().str())) {
        for (unsigned i = 0; i < call->getNumOperands() - 1; ++i) {
            if (call->getOperand(i) == fact.get_value()) {
                return true;
            }
        }
    }
    
    return false;
}

// Helper function to handle source function specifications from config
void TaintAnalysis::handle_source_function_specs(const llvm::CallInst* call, FactSet& result) const {
    std::string func_name = taint_config::normalize_name(call->getCalledFunction()->getName().str());
    const FunctionTaintConfig* func_config = taint_config::get_function_config(func_name);

    if (func_config && func_config->has_source_specs()) {
        for (const auto& spec : func_config->source_specs) {
            if (spec.location == TaintSpec::RET && spec.access_mode == TaintSpec::VALUE) {
                result.insert(TaintFact::tainted_var(call));
            } else if (spec.location == TaintSpec::ARG && spec.access_mode == TaintSpec::DEREF) {
                if (spec.arg_index >= 0 && spec.arg_index < (int)(call->getNumOperands() - 1)) {
                    const llvm::Value* arg = call->getOperand(spec.arg_index);
                    if (arg->getType()->isPointerTy()) {
                        result.insert(TaintFact::tainted_memory(arg));
                    }
                }
            } else if (spec.location == TaintSpec::AFTER_ARG && spec.access_mode == TaintSpec::DEREF) {
                unsigned start_arg = spec.arg_index + 1;
                for (unsigned i = start_arg; i < call->getNumOperands() - 1; ++i) {
                    const llvm::Value* arg = call->getOperand(i);
                    if (arg->getType()->isPointerTy()) {
                        result.insert(TaintFact::tainted_memory(arg));
                    }
                }
            }
        }
    }
}

// Helper function to handle PIPE specifications for taint propagation
void TaintAnalysis::handle_pipe_specifications(const llvm::CallInst* call, const TaintFact& fact, FactSet& result) const {
    std::string func_name = taint_config::normalize_name(call->getCalledFunction()->getName().str());
    const FunctionTaintConfig* func_config = taint_config::get_function_config(func_name);

    if (func_config && func_config->has_pipe_specs()) {
        for (const auto& pipe_spec : func_config->pipe_specs) {
            bool matches_from = false;

            // Check if current fact matches the 'from' spec
            if (pipe_spec.from.location == TaintSpec::ARG) {
                int from_arg_idx = pipe_spec.from.arg_index;
                if (from_arg_idx >= 0 && from_arg_idx < (int)(call->getNumOperands() - 1)) {
                    const llvm::Value* from_arg = call->getOperand(from_arg_idx);

                    if (pipe_spec.from.access_mode == TaintSpec::VALUE) {
                        if (fact.is_tainted_var() && fact.get_value() == from_arg) {
                            matches_from = true;
                        }
                    } else {
                        if (fact.is_tainted_memory() && from_arg->getType()->isPointerTy()) {
                            if (taint_may_alias(from_arg, fact.get_memory_location())) {
                                matches_from = true;
                            }
                        }
                    }
                }
            }

            // If from matches, propagate to 'to'
            if (matches_from) {
                if (pipe_spec.to.location == TaintSpec::RET) {
                    if (pipe_spec.to.access_mode == TaintSpec::VALUE) {
                        result.insert(TaintFact::tainted_var(call));
                    } else {
                        if (call->getType()->isPointerTy()) {
                            result.insert(TaintFact::tainted_memory(call));
                        }
                    }
                } else if (pipe_spec.to.location == TaintSpec::ARG) {
                    int to_arg_idx = pipe_spec.to.arg_index;
                    if (to_arg_idx >= 0 && to_arg_idx < (int)(call->getNumOperands() - 1)) {
                        const llvm::Value* to_arg = call->getOperand(to_arg_idx);

                        if (pipe_spec.to.access_mode == TaintSpec::VALUE) {
                            result.insert(TaintFact::tainted_var(to_arg));
                        } else {
                            if (to_arg->getType()->isPointerTy()) {
                            result.insert(TaintFact::tainted_memory(to_arg));
                            }
                        }
                    }
                }
            }
        }
    }
}

// Helper function to check if an argument is tainted by a fact
bool TaintAnalysis::is_argument_tainted(const llvm::Value* arg, const TaintFact& fact) const {
    return (fact.is_tainted_var() && fact.get_value() == arg) ||
           (fact.is_tainted_memory() && arg->getType()->isPointerTy() &&
            (fact.get_memory_location() == arg || may_alias(arg, fact.get_memory_location())));
}

// Helper function to format tainted argument description
std::string TaintAnalysis::format_tainted_arg(unsigned arg_index, const TaintFact& fact, const llvm::CallInst* call) const {
    if (fact.is_tainted_var()) {
        return "arg" + std::to_string(arg_index);
    } else if (fact.is_tainted_memory()) {
        return (fact.get_memory_location() == call->getOperand(arg_index)) ?
            "arg" + std::to_string(arg_index) + "(mem)" :
            "arg" + std::to_string(arg_index) + "(alias)";
    }
    return "";
}

// Helper function to analyze tainted arguments for a call
void TaintAnalysis::analyze_tainted_arguments(const llvm::CallInst* call, const TaintAnalysis::FactSet& facts,
                              std::string& tainted_args) const {
    std::set<std::string> unique_tainted_args;

    for (unsigned i = 0; i < call->getNumOperands() - 1; ++i) {
        const llvm::Value* arg = call->getOperand(i);

        for (const auto& fact : facts) {
            if (is_argument_tainted(arg, fact)) {
                std::string arg_desc = format_tainted_arg(i, fact, call);
                if (!arg_desc.empty()) {
                    unique_tainted_args.insert(arg_desc);
                }
                break;
            }
        }
    }

    // Join unique tainted arguments
    for (const auto& arg_desc : unique_tainted_args) {
        if (!tainted_args.empty()) tainted_args += ", ";
        tainted_args += arg_desc;
    }
}

} // namespace ifds
