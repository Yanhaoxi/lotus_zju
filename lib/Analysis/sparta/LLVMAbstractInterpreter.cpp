/*
 * LLVM Abstract Interpreter Implementation
 * Main interpreter functionality and module analysis
 */

#include <Analysis/sparta/LLVMAbstractInterpreter.h>

#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Value.h>

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <functional>

namespace sparta {
namespace llvm_ai {

// ============================================================================
// LLVMAbstractInterpreter Implementation
// ============================================================================

void LLVMAbstractInterpreter::analyze_function(const llvm::Function* function) {
    auto iterator = std::make_unique<LLVMFixpointIterator>(function);
    
    // Set up initial state
    LLVMAbstractState initial_state = LLVMAbstractState::top();
    
    // Initialize function parameters
    for (const auto& arg : function->args()) {
        initial_state.set_value(&arg, LLVMValueDomain::from_llvm_value(&arg));
    }
    
    // Run fixpoint iteration
    iterator->run(initial_state);
    
    // Store results
    for (const auto& block : *function) {
        m_block_states[&block] = iterator->get_entry_state_at(&block);
    }
    
    m_function_analyses[function] = std::move(iterator);
}

void LLVMAbstractInterpreter::analyze_function_with_context(const llvm::Function* function, const AnalysisContext& /*context*/) {
    // Context-sensitive analysis implementation
    // For now, fall back to context-insensitive analysis
    analyze_function(function);
}

void LLVMAbstractInterpreter::analyze_module(const llvm::Module* module) {
    for (const auto& function : *module) {
        if (!function.isDeclaration()) {
            analyze_function(&function);
        }
    }
}

void LLVMAbstractInterpreter::analyze_module_interprocedural(const llvm::Module* module) {
    // Build call graph first
    build_call_graph(module);
    
    // Analyze functions in topological order
    auto topo_order = m_call_graph.get_topological_order();
    
    for (const llvm::Function* function : topo_order) {
        if (!function->isDeclaration()) {
            analyze_function(function);
        }
    }
}

LLVMAbstractState LLVMAbstractInterpreter::get_state_at_block_entry(const llvm::BasicBlock* block) const {
    auto it = m_block_states.find(block);
    if (it != m_block_states.end()) {
        return it->second;
    }
    return LLVMAbstractState::bottom();
}

LLVMAbstractState LLVMAbstractInterpreter::get_state_at_block_exit(const llvm::BasicBlock* block) const {
    auto function = block->getParent();
    auto it = m_function_analyses.find(function);
    if (it != m_function_analyses.end()) {
        return it->second->get_exit_state_at(block);
    }
    return LLVMAbstractState::bottom();
}

LLVMValueDomain LLVMAbstractInterpreter::get_value_at_instruction(const llvm::Instruction* inst, const llvm::Value* value) const {
    // This would require instruction-level state tracking
    // For now, return the value at block entry
    LLVMAbstractState state = get_state_at_block_entry(inst->getParent());
    return state.get_value(value);
}

void LLVMAbstractInterpreter::print_analysis_results(const llvm::Function* function, std::ostream& os) const {
    os << "Analysis results for function: " << function->getName().str() << "\n";
    os << "========================================\n";
    
    for (const auto& block : *function) {
        os << "Block: " << block.getName().str() << "\n";
        LLVMAbstractState entry_state = get_state_at_block_entry(&block);
        os << "Entry state: " << entry_state << "\n";
        
        LLVMAbstractState exit_state = get_state_at_block_exit(&block);
        os << "Exit state: " << exit_state << "\n";
        os << "----------------------------------------\n";
    }
}

void LLVMAbstractInterpreter::clear_analysis_results() {
    m_function_analyses.clear();
    m_block_states.clear();
    m_context_sensitive_results.clear();
    m_function_summaries.clear();
}

// ============================================================================
// Call Graph and Interprocedural Analysis
// ============================================================================

void LLVMAbstractInterpreter::build_call_graph(const llvm::Module* module) {
    m_call_graph = LLVMCallGraph(); // Reset call graph
    
    for (const auto& function : *module) {
        if (function.isDeclaration()) continue;
        
        for (const auto& block : function) {
            for (const auto& inst : block) {
                if (auto* call = llvm::dyn_cast<llvm::CallInst>(&inst)) {
                    const llvm::Function* callee = call->getCalledFunction();
                    if (callee) {
                        m_call_graph.add_call_edge(call, &function, callee);
                    }
                }
            }
        }
    }
}

LLVMAbstractState LLVMAbstractInterpreter::analyze_function_call(const llvm::CallInst* call, const LLVMAbstractState& /*caller_state*/, const AnalysisContext& context) {
    const llvm::Function* callee = call->getCalledFunction();
    if (!callee || callee->isDeclaration()) {
        return LLVMAbstractState::top();
    }
    
    // Check if we have a cached result for this context
    size_t context_hash = context.hash();
    auto it = m_context_sensitive_results.find(context_hash);
    if (it != m_context_sensitive_results.end()) {
        return it->second;
    }
    
    // Analyze the callee with the given context
    analyze_function_with_context(callee, context);
    
    // Get the result and cache it
    LLVMAbstractState result = get_state_at_block_exit(&callee->getEntryBlock());
    m_context_sensitive_results[context_hash] = result;
    
    return result;
}

void LLVMAbstractInterpreter::update_memory_model(const llvm::Instruction* inst, LLVMAbstractState& state) const {
    // Enhanced memory modeling for specific instructions
    if (auto* load = llvm::dyn_cast<llvm::LoadInst>(inst)) {
        // Load instruction - model memory read
        const llvm::Value* ptr = load->getPointerOperand();
        LLVMValueDomain loaded_value = state.load_memory(ptr);
        state.set_value(load, loaded_value);
    } else if (auto* store = llvm::dyn_cast<llvm::StoreInst>(inst)) {
        // Store instruction - model memory write
        const llvm::Value* value = store->getValueOperand();
        const llvm::Value* ptr = store->getPointerOperand();
        LLVMValueDomain stored_value = state.get_value(value);
        state.store_memory(ptr, stored_value);
    }
    // Add more memory modeling as needed
}

// ============================================================================
// LLVMCallGraph Implementation
// ============================================================================

void LLVMCallGraph::add_call_edge(const llvm::CallInst* call_inst, const llvm::Function* caller, const llvm::Function* callee) {
    m_call_sites.emplace_back(call_inst, caller, callee);
    LLVMCallGraph::CallSite& site = m_call_sites.back();
    
    m_caller_map[callee].push_back(&site);
    m_callee_map[caller].push_back(&site);
}

std::vector<LLVMCallGraph::CallSite> LLVMCallGraph::get_call_sites(const llvm::Function* function) const {
    std::vector<CallSite> result;
    auto it = m_callee_map.find(function);
    if (it != m_callee_map.end()) {
        for (const CallSite* site : it->second) {
            result.push_back(*site);
        }
    }
    return result;
}

std::vector<const llvm::Function*> LLVMCallGraph::get_callers(const llvm::Function* function) const {
    std::vector<const llvm::Function*> result;
    auto it = m_caller_map.find(function);
    if (it != m_caller_map.end()) {
        for (const CallSite* site : it->second) {
            result.push_back(site->caller);
        }
    }
    return result;
}

std::vector<const llvm::Function*> LLVMCallGraph::get_callees(const llvm::Function* function) const {
    std::vector<const llvm::Function*> result;
    auto it = m_callee_map.find(function);
    if (it != m_callee_map.end()) {
        for (const CallSite* site : it->second) {
            result.push_back(site->callee);
        }
    }
    return result;
}

std::vector<const llvm::Function*> LLVMCallGraph::get_topological_order() const {
    // Simple topological sort implementation
    std::vector<const llvm::Function*> result;
    std::unordered_set<const llvm::Function*> visited;
    std::unordered_set<const llvm::Function*> temp_visited;
    
    std::function<void(const llvm::Function*)> visit = [&](const llvm::Function* func) {
        if (temp_visited.count(func)) {
            // Cycle detected - skip
            return;
        }
        if (visited.count(func)) {
            return;
        }
        
        temp_visited.insert(func);
        
        // Visit all callees first
        for (const llvm::Function* callee : get_callees(func)) {
            visit(callee);
        }
        
        temp_visited.erase(func);
        visited.insert(func);
        result.push_back(func);
    };
    
    // Visit all functions
    for (const auto& pair : m_callee_map) {
        visit(pair.first);
    }
    
    return result;
}

// ============================================================================
// AnalysisContext Implementation
// ============================================================================

bool AnalysisContext::CallContext::operator==(const CallContext& other) const {
    return function == other.function && 
           arguments == other.arguments && 
           call_site == other.call_site;
}

size_t AnalysisContext::CallContext::hash() const {
    size_t h = std::hash<const llvm::Function*>{}(function);
    h ^= std::hash<const llvm::CallInst*>{}(call_site) + 0x9e3779b9 + (h << 6) + (h >> 2);
    
    for (const auto& arg : arguments) {
        // Simple hash for LLVMValueDomain - in practice, you'd want a proper hash function
        h ^= std::hash<int64_t>{}(arg.is_constant() ? arg.get_constant().value_or(0) : 0) + 0x9e3779b9 + (h << 6) + (h >> 2);
    }
    
    return h;
}

bool AnalysisContext::equals(const AnalysisContext& other) const {
    return call_stack == other.call_stack;
}

size_t AnalysisContext::hash() const {
    size_t h = 0;
    for (const auto& context : call_stack) {
        h ^= context.hash() + 0x9e3779b9 + (h << 6) + (h >> 2);
    }
    return h;
}

} // namespace llvm_ai
} // namespace sparta
