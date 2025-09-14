/*
 * LLVM Fixpoint Iterator Implementation
 * Fixpoint iteration logic for LLVM abstract interpreter
 */

#include <Analysis/sparta/LLVMAbstractInterpreter.h>

#include <llvm/IR/CFG.h>
#include <llvm/IR/Instructions.h>

#include <queue>
#include <unordered_set>
#include <unordered_map>

namespace sparta {
namespace llvm_ai {

// ============================================================================
// LLVMControlFlowGraph Implementation
// ============================================================================

std::vector<LLVMControlFlowGraph::EdgeId> 
LLVMControlFlowGraph::predecessors(const Graph& /*graph*/, const NodeId& node) {
    std::vector<EdgeId> preds;
    for (auto it = llvm::pred_begin(node), end = llvm::pred_end(node); it != end; ++it) {
        preds.emplace_back(*it, node);
    }
    return preds;
}

std::vector<LLVMControlFlowGraph::EdgeId> 
LLVMControlFlowGraph::successors(const Graph& /*graph*/, const NodeId& node) {
    std::vector<EdgeId> succs;
    for (auto it = llvm::succ_begin(node), end = llvm::succ_end(node); it != end; ++it) {
        succs.emplace_back(node, *it);
    }
    return succs;
}

// ============================================================================
// LLVMFixpointIterator Implementation
// ============================================================================

void LLVMFixpointIterator::analyze_node(const llvm::BasicBlock* const& node,
                                       LLVMAbstractState* current_state) const {
    for (const auto& inst : *node) {
        LLVMTransferFunction::apply_instruction(&inst, *current_state);
    }
}

LLVMAbstractState LLVMFixpointIterator::analyze_edge(
    const std::pair<const llvm::BasicBlock*, const llvm::BasicBlock*>& edge,
    const LLVMAbstractState& exit_state_at_source) const {
    
    const llvm::BasicBlock* source = edge.first;
    const llvm::BasicBlock* target = edge.second;
    
    // Handle branch conditions
    if (auto* br = llvm::dyn_cast<llvm::BranchInst>(source->getTerminator())) {
        if (br->isConditional()) {
            auto [true_state, false_state] = LLVMTransferFunction::analyze_branch_condition(br, exit_state_at_source);
            
            if (br->getSuccessor(0) == target) {
                return true_state;
            } else if (br->getSuccessor(1) == target) {
                return false_state;
            }
        }
    }
    
    // Handle PHI nodes in the target block
    LLVMAbstractState result = exit_state_at_source;
    
    for (const auto& inst : *target) {
        if (auto* phi = llvm::dyn_cast<llvm::PHINode>(&inst)) {
            for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
                if (phi->getIncomingBlock(i) == source) {
                    LLVMValueDomain incoming_value = result.get_value(phi->getIncomingValue(i));
                    result.set_value(phi, incoming_value);
                    break;
                }
            }
        } else {
            break; // PHI nodes are always at the beginning of a block
        }
    }
    
    return result;
}

void LLVMFixpointIterator::run(const LLVMAbstractState& initial_state) {
    // Real fixpoint iteration with convergence checking
    const size_t MAX_ITERATIONS = 1000;
    const size_t WIDENING_THRESHOLD = 5;
    
    // Initialize all blocks to bottom except entry block
    const llvm::BasicBlock* entry_block = &m_function->getEntryBlock();
    for (const auto& block : *m_function) {
        if (&block == entry_block) {
            m_entry_states[&block] = initial_state;
        } else {
            m_entry_states[&block] = LLVMAbstractState::bottom();
        }
        m_exit_states[&block] = LLVMAbstractState::bottom();
    }
    
    // Track iterations per block for widening
    std::unordered_map<const llvm::BasicBlock*, size_t> block_iterations;
    
    // Worklist algorithm for fixpoint iteration
    std::queue<const llvm::BasicBlock*> worklist;
    std::unordered_set<const llvm::BasicBlock*> in_worklist;
    
    // Add entry block to worklist
    worklist.push(entry_block);
    in_worklist.insert(entry_block);
    
    size_t global_iterations = 0;
    bool converged = false;
    
    while (!worklist.empty() && global_iterations < MAX_ITERATIONS && !converged) {
        const llvm::BasicBlock* current_block = worklist.front();
        worklist.pop();
        in_worklist.erase(current_block);
        
        // Get current entry state
        LLVMAbstractState entry_state = m_entry_states[current_block];
        
        // Apply transfer function to get exit state
        LLVMAbstractState exit_state = entry_state;
        for (const auto& inst : *current_block) {
            LLVMTransferFunction::apply_instruction(&inst, exit_state);
        }
        
        // Check if exit state changed
        LLVMAbstractState old_exit_state = m_exit_states[current_block];
        bool exit_changed = !exit_state.equals(old_exit_state);
        
        if (exit_changed) {
            m_exit_states[current_block] = exit_state;
            
            // Propagate to successors
            for (const llvm::BasicBlock* succ : llvm::successors(current_block)) {
                // Compute edge transfer
                LLVMAbstractState edge_state = analyze_edge({current_block, succ}, exit_state);
                
                // Join with existing entry state of successor
                LLVMAbstractState old_succ_entry = m_entry_states[succ];
                LLVMAbstractState new_succ_entry = old_succ_entry;
                new_succ_entry.join_with(edge_state);
                
                // Apply widening if we've iterated too many times
                block_iterations[succ]++;
                if (block_iterations[succ] > WIDENING_THRESHOLD) {
                    new_succ_entry = old_succ_entry.widening(edge_state);
                }
                
                // Check if successor entry state changed
                if (!new_succ_entry.equals(old_succ_entry)) {
                    m_entry_states[succ] = new_succ_entry;
                    
                    // Add successor to worklist if not already there
                    if (in_worklist.find(succ) == in_worklist.end()) {
                        worklist.push(succ);
                        in_worklist.insert(succ);
                    }
                }
            }
        }
        
        global_iterations++;
        
        // Check for convergence (worklist empty means no more changes)
        if (worklist.empty()) {
            converged = true;
        }
    }
    
    // Apply narrowing for precision refinement
    if (converged) {
        apply_narrowing();
    }
}

LLVMAbstractState LLVMFixpointIterator::get_entry_state_at(const llvm::BasicBlock* block) const {
    auto it = m_entry_states.find(block);
    if (it != m_entry_states.end()) {
        return it->second;
    }
    return LLVMAbstractState::bottom();
}

LLVMAbstractState LLVMFixpointIterator::get_exit_state_at(const llvm::BasicBlock* block) const {
    auto it = m_exit_states.find(block);
    if (it != m_exit_states.end()) {
        return it->second;
    }
    return LLVMAbstractState::bottom();
}

void LLVMFixpointIterator::apply_narrowing() {
    // Apply narrowing operator for precision refinement
    const size_t MAX_NARROWING_ITERATIONS = 10;
    
    std::queue<const llvm::BasicBlock*> worklist;
    std::unordered_set<const llvm::BasicBlock*> in_worklist;
    
    // Add all blocks to worklist for narrowing
    for (const auto& block : *m_function) {
        worklist.push(&block);
        in_worklist.insert(&block);
    }
    
    size_t narrowing_iterations = 0;
    
    while (!worklist.empty() && narrowing_iterations < MAX_NARROWING_ITERATIONS) {
        const llvm::BasicBlock* current_block = worklist.front();
        worklist.pop();
        in_worklist.erase(current_block);
        
        // Compute new entry state by joining predecessors
        LLVMAbstractState new_entry_state = LLVMAbstractState::bottom();
        bool first_pred = true;
        
        for (const llvm::BasicBlock* pred : llvm::predecessors(current_block)) {
            LLVMAbstractState pred_exit = m_exit_states[pred];
            LLVMAbstractState edge_state = analyze_edge({pred, current_block}, pred_exit);
            
            if (first_pred) {
                new_entry_state = edge_state;
                first_pred = false;
            } else {
                new_entry_state.join_with(edge_state);
            }
        }
        
        // Apply narrowing with current entry state
        LLVMAbstractState old_entry_state = m_entry_states[current_block];
        LLVMAbstractState narrowed_state = old_entry_state.narrowing(new_entry_state);
        
        // Check if state changed
        if (!narrowed_state.equals(old_entry_state)) {
            m_entry_states[current_block] = narrowed_state;
            
            // Recompute exit state
            LLVMAbstractState exit_state = narrowed_state;
            for (const auto& inst : *current_block) {
                LLVMTransferFunction::apply_instruction(&inst, exit_state);
            }
            m_exit_states[current_block] = exit_state;
            
            // Add successors to worklist
            for (const llvm::BasicBlock* succ : llvm::successors(current_block)) {
                if (in_worklist.find(succ) == in_worklist.end()) {
                    worklist.push(succ);
                    in_worklist.insert(succ);
                }
            }
        }
        
        narrowing_iterations++;
    }
}

} // namespace llvm_ai
} // namespace sparta
