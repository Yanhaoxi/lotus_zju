/*
 * LLVM IR Abstract Interpreter using Sparta Framework
 * 
 * This header provides a comprehensive abstract interpreter for LLVM IR
 * built on top of the Sparta abstract interpretation library.
 */

#pragma once

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/CFG.h>

#include <Analysis/sparta/AbstractDomain.h>
#include <Analysis/sparta/AbstractEnvironment.h>
#include <Analysis/sparta/AbstractMap.h>
#include <Analysis/sparta/AbstractMapValue.h>
#include <Analysis/sparta/ConstantAbstractDomain.h>
#include <Analysis/sparta/IntervalDomain.h>
#include <Analysis/sparta/PatriciaTreeMap.h>
#include <Analysis/sparta/PatriciaTreeMapAbstractEnvironment.h>
#include <Analysis/sparta/FixpointIterator.h>
#include <Analysis/sparta/MonotonicFixpointIterator.h>
#include <Analysis/sparta/Exceptions.h>
#include <Analysis/sparta/TypeTraits.h>

#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace sparta {
namespace llvm_ai {

// Forward declarations
class LLVMValueDomain;
class LLVMAbstractState;
class LLVMTransferFunction;
class LLVMControlFlowGraph;

/*
 * Abstract domain for LLVM values
 * Combines constant propagation with interval analysis for integers
 * and supports pointer analysis basics
 */
class LLVMValueDomain final : public AbstractDomain<LLVMValueDomain> {
public:
    enum class ValueKind {
        Bottom,    // Unreachable/undefined
        Constant,  // Known constant value
        Interval,  // Integer interval [low, high]
        Pointer,   // Pointer to memory location
        Unknown,   // Top - could be anything
        Top        // Alias for Unknown
    };

private:
    ValueKind m_kind;
    
    // Storage for different value types
    union {
        int64_t m_int_constant;
        struct {
            int64_t m_low;
            int64_t m_high;
        } m_interval;
        const llvm::Value* m_pointer_base;
    };
    
public:
    LLVMValueDomain() : m_kind(ValueKind::Top) {}
    
    explicit LLVMValueDomain(int64_t constant) 
        : m_kind(ValueKind::Constant), m_int_constant(constant) {}
    
    explicit LLVMValueDomain(int64_t low, int64_t high) 
        : m_kind(ValueKind::Interval) {
        m_interval.m_low = low;
        m_interval.m_high = high;
        if (low == high) {
            m_kind = ValueKind::Constant;
            m_int_constant = low;
        }
    }
    
    explicit LLVMValueDomain(const llvm::Value* ptr) 
        : m_kind(ValueKind::Pointer), m_pointer_base(ptr) {}
    
    static LLVMValueDomain bottom() {
        LLVMValueDomain result;
        result.m_kind = ValueKind::Bottom;
        return result;
    }
    
    static LLVMValueDomain top() {
        return LLVMValueDomain();
    }
    
    bool is_bottom() const { return m_kind == ValueKind::Bottom; }
    bool is_top() const { return m_kind == ValueKind::Top || m_kind == ValueKind::Unknown; }
    
    bool is_constant() const { return m_kind == ValueKind::Constant; }
    bool is_interval() const { return m_kind == ValueKind::Interval; }
    bool is_pointer() const { return m_kind == ValueKind::Pointer; }
    
    boost::optional<int64_t> get_constant() const {
        if (m_kind == ValueKind::Constant) {
            return m_int_constant;
        }
        return boost::none;
    }
    
    boost::optional<std::pair<int64_t, int64_t>> get_interval() const {
        if (m_kind == ValueKind::Interval) {
            return std::make_pair(m_interval.m_low, m_interval.m_high);
        } else if (m_kind == ValueKind::Constant) {
            return std::make_pair(m_int_constant, m_int_constant);
        }
        return boost::none;
    }
    
    const llvm::Value* get_pointer_base() const {
        return (m_kind == ValueKind::Pointer) ? m_pointer_base : nullptr;
    }
    
    void set_to_bottom() { m_kind = ValueKind::Bottom; }
    void set_to_top() { m_kind = ValueKind::Top; }
    
    bool leq(const LLVMValueDomain& other) const;
    bool equals(const LLVMValueDomain& other) const;
    void join_with(const LLVMValueDomain& other);
    void widen_with(const LLVMValueDomain& other);
    void meet_with(const LLVMValueDomain& other);
    void narrow_with(const LLVMValueDomain& other);
    
    // Arithmetic operations
    LLVMValueDomain add(const LLVMValueDomain& other) const;
    LLVMValueDomain sub(const LLVMValueDomain& other) const;
    LLVMValueDomain mul(const LLVMValueDomain& other) const;
    LLVMValueDomain div(const LLVMValueDomain& other) const;
    
    // Comparison operations
    LLVMValueDomain icmp_eq(const LLVMValueDomain& other) const;
    LLVMValueDomain icmp_ne(const LLVMValueDomain& other) const;
    LLVMValueDomain icmp_slt(const LLVMValueDomain& other) const;
    LLVMValueDomain icmp_sle(const LLVMValueDomain& other) const;
    
    // Factory methods for LLVM constants
    static LLVMValueDomain from_llvm_constant(const llvm::Constant* c);
    static LLVMValueDomain from_llvm_value(const llvm::Value* v);
    
    friend std::ostream& operator<<(std::ostream& os, const LLVMValueDomain& domain);
};

/*
 * Memory domain for tracking heap and stack locations
 * Maps memory locations to abstract values
 */
using MemoryLocation = const llvm::Value*;
using MemoryMap = PatriciaTreeMap<MemoryLocation, LLVMValueDomain>;
using LLVMMemoryDomain = PatriciaTreeMapAbstractEnvironment<MemoryLocation, LLVMValueDomain>;

/*
 * Abstract state combining value and memory domains
 * Maps LLVM values to their abstract representations
 */
using ValueMap = PatriciaTreeMap<const llvm::Value*, LLVMValueDomain>;
using LLVMValueEnvironment = PatriciaTreeMapAbstractEnvironment<const llvm::Value*, LLVMValueDomain>;

class LLVMAbstractState final : public AbstractDomain<LLVMAbstractState> {
private:
    LLVMValueEnvironment m_values;
    LLVMMemoryDomain m_memory;
    
public:
    LLVMAbstractState() = default;
    
    static LLVMAbstractState bottom() {
        LLVMAbstractState result;
        result.m_values = LLVMValueEnvironment::bottom();
        result.m_memory = LLVMMemoryDomain::bottom();
        return result;
    }
    
    static LLVMAbstractState top() {
        return LLVMAbstractState();
    }
    
    bool is_bottom() const { 
        return m_values.is_bottom() || m_memory.is_bottom(); 
    }
    
    bool is_top() const { 
        return m_values.is_top() && m_memory.is_top(); 
    }
    
    void set_to_bottom() {
        m_values.set_to_bottom();
        m_memory.set_to_bottom();
    }
    
    void set_to_top() {
        m_values.set_to_top();
        m_memory.set_to_top();
    }
    
    bool equals(const LLVMAbstractState& other) const {
        return m_values.equals(other.m_values) && m_memory.equals(other.m_memory);
    }
    
    void join_with(const LLVMAbstractState& other) {
        m_values.join_with(other.m_values);
        m_memory.join_with(other.m_memory);
    }
    
    LLVMAbstractState widening(const LLVMAbstractState& other) const {
        LLVMAbstractState result;
        result.m_values = m_values.widening(other.m_values);
        result.m_memory = m_memory.widening(other.m_memory);
        return result;
    }
    
    LLVMAbstractState narrowing(const LLVMAbstractState& other) const {
        LLVMAbstractState result;
        result.m_values = m_values.narrowing(other.m_values);
        result.m_memory = m_memory.narrowing(other.m_memory);
        return result;
    }
    
    bool leq(const LLVMAbstractState& other) const {
        return m_values.leq(other.m_values) && m_memory.leq(other.m_memory);
    }
    
    void widen_with(const LLVMAbstractState& other) {
        m_values.widen_with(other.m_values);
        m_memory.widen_with(other.m_memory);
    }
    
    void meet_with(const LLVMAbstractState& other) {
        m_values.meet_with(other.m_values);
        m_memory.meet_with(other.m_memory);
    }
    
    void narrow_with(const LLVMAbstractState& other) {
        m_values.narrow_with(other.m_values);
        m_memory.narrow_with(other.m_memory);
    }
    
    // Value access and manipulation
    LLVMValueDomain get_value(const llvm::Value* v) const {
        if (is_bottom()) return LLVMValueDomain::bottom();
        return m_values.get(v);
    }
    
    void set_value(const llvm::Value* v, const LLVMValueDomain& domain) {
        if (is_bottom()) return;
        m_values.set(v, domain);
    }
    
    // Memory access and manipulation
    LLVMValueDomain load_memory(const llvm::Value* ptr) const {
        if (is_bottom()) return LLVMValueDomain::bottom();
        return m_memory.get(ptr);
    }
    
    void store_memory(const llvm::Value* ptr, const LLVMValueDomain& value) {
        if (is_bottom()) return;
        m_memory.set(ptr, value);
    }
    
    void invalidate_memory(const llvm::Value* ptr) {
        if (is_bottom()) return;
        m_memory.set(ptr, LLVMValueDomain::top());
    }
    
    // Utility methods
    const LLVMValueEnvironment& get_value_environment() const { return m_values; }
    const LLVMMemoryDomain& get_memory_domain() const { return m_memory; }
    
    friend std::ostream& operator<<(std::ostream& os, const LLVMAbstractState& state);
};

/*
 * Transfer functions for LLVM instructions
 * Implements the abstract semantics of LLVM IR
 */
class LLVMTransferFunction {
public:
    // Main transfer function dispatcher
    static void apply_instruction(const llvm::Instruction* inst, LLVMAbstractState& state);
    
    // Specific instruction handlers
    static void handle_binary_operator(const llvm::BinaryOperator* binop, LLVMAbstractState& state);
    static void handle_icmp(const llvm::ICmpInst* icmp, LLVMAbstractState& state);
    static void handle_load(const llvm::LoadInst* load, LLVMAbstractState& state);
    static void handle_store(const llvm::StoreInst* store, LLVMAbstractState& state);
    static void handle_alloca(const llvm::AllocaInst* alloca, LLVMAbstractState& state);
    static void handle_call(const llvm::CallInst* call, LLVMAbstractState& state);
    static void handle_phi(const llvm::PHINode* phi, LLVMAbstractState& state);
    static void handle_cast(const llvm::CastInst* cast, LLVMAbstractState& state);
    static void handle_gep(const llvm::GetElementPtrInst* gep, LLVMAbstractState& state);
    
    // Branch condition analysis
    static std::pair<LLVMAbstractState, LLVMAbstractState> 
    analyze_branch_condition(const llvm::BranchInst* br, const LLVMAbstractState& state);
    
    // Enhanced call handling
    static void handle_library_function_call(const llvm::CallInst* call, LLVMAbstractState& state);
    static void model_call_side_effects(const llvm::CallInst* call, LLVMAbstractState& state);
};

/*
 * Control Flow Graph interface for LLVM basic blocks
 */
class LLVMControlFlowGraph {
public:
    using Graph = const llvm::Function*;
    using NodeId = const llvm::BasicBlock*;
    using EdgeId = std::pair<const llvm::BasicBlock*, const llvm::BasicBlock*>;
    
    static NodeId entry(const Graph& graph) {
        return &graph->getEntryBlock();
    }
    
    static NodeId source(const Graph& graph, const EdgeId& edge) {
        return edge.first;
    }
    
    static NodeId target(const Graph& graph, const EdgeId& edge) {
        return edge.second;
    }
    
    static std::vector<EdgeId> predecessors(const Graph& graph, const NodeId& node);
    static std::vector<EdgeId> successors(const Graph& graph, const NodeId& node);
};

/*
 * Fixpoint iterator for LLVM functions
 */
class LLVMFixpointIterator : public FixpointIterator<LLVMControlFlowGraph, LLVMAbstractState> {
public:
    explicit LLVMFixpointIterator(const llvm::Function* function)
        : m_function(function) {}
    
    void analyze_node(const llvm::BasicBlock* const& node,
                     LLVMAbstractState* current_state) const override;
    
    LLVMAbstractState analyze_edge(const std::pair<const llvm::BasicBlock*, const llvm::BasicBlock*>& edge,
                                  const LLVMAbstractState& exit_state_at_source) const override;
    
    // Additional methods for state querying
    void run(const LLVMAbstractState& initial_state);
    LLVMAbstractState get_entry_state_at(const llvm::BasicBlock* block) const;
    LLVMAbstractState get_exit_state_at(const llvm::BasicBlock* block) const;
    
private:
    void apply_narrowing();
    
    const llvm::Function* m_function;
    std::unordered_map<const llvm::BasicBlock*, LLVMAbstractState> m_entry_states;
    std::unordered_map<const llvm::BasicBlock*, LLVMAbstractState> m_exit_states;
};

/*
 * Main abstract interpreter class
 */
/*
 * Call graph for interprocedural analysis
 */
class LLVMCallGraph {
public:
    struct CallSite {
        const llvm::CallInst* call_inst;
        const llvm::Function* caller;
        const llvm::Function* callee;
        
        CallSite(const llvm::CallInst* ci, const llvm::Function* caller_func, const llvm::Function* callee_func)
            : call_inst(ci), caller(caller_func), callee(callee_func) {}
    };
    
    void add_call_edge(const llvm::CallInst* call_inst, const llvm::Function* caller, const llvm::Function* callee);
    std::vector<CallSite> get_call_sites(const llvm::Function* function) const;
    std::vector<const llvm::Function*> get_callers(const llvm::Function* function) const;
    std::vector<const llvm::Function*> get_callees(const llvm::Function* function) const;
    std::vector<const llvm::Function*> get_topological_order() const;
    
private:
    std::vector<CallSite> m_call_sites;
    std::unordered_map<const llvm::Function*, std::vector<CallSite*>> m_caller_map;
    std::unordered_map<const llvm::Function*, std::vector<CallSite*>> m_callee_map;
};

/*
 * Context for interprocedural analysis
 */
class AnalysisContext {
public:
    struct CallContext {
        const llvm::Function* function;
        std::vector<LLVMValueDomain> arguments;
        const llvm::CallInst* call_site;
        
        CallContext(const llvm::Function* func, const std::vector<LLVMValueDomain>& args, const llvm::CallInst* cs)
            : function(func), arguments(args), call_site(cs) {}
        
        bool operator==(const CallContext& other) const;
        size_t hash() const;
    };
    
    std::vector<CallContext> call_stack;
    
    bool equals(const AnalysisContext& other) const;
    size_t hash() const;
};

class LLVMAbstractInterpreter {
private:
    std::unordered_map<const llvm::Function*, std::unique_ptr<LLVMFixpointIterator>> m_function_analyses;
    std::unordered_map<const llvm::BasicBlock*, LLVMAbstractState> m_block_states;
    
    // Interprocedural analysis support
    LLVMCallGraph m_call_graph;
    std::unordered_map<size_t, LLVMAbstractState> m_context_sensitive_results; // context hash -> result
    std::unordered_map<const llvm::Function*, LLVMAbstractState> m_function_summaries;
    
public:
    // Analyze a single function
    void analyze_function(const llvm::Function* function);
    void analyze_function_with_context(const llvm::Function* function, const AnalysisContext& context);
    
    // Analyze an entire module
    void analyze_module(const llvm::Module* module);
    void analyze_module_interprocedural(const llvm::Module* module);
    
    // Query results
    LLVMAbstractState get_state_at_block_entry(const llvm::BasicBlock* block) const;
    LLVMAbstractState get_state_at_block_exit(const llvm::BasicBlock* block) const;
    LLVMValueDomain get_value_at_instruction(const llvm::Instruction* inst, const llvm::Value* value) const;
    
    // Utility methods
    void print_analysis_results(const llvm::Function* function, std::ostream& os) const;
    void clear_analysis_results();
    
    // Pointer analysis and memory modeling
    void build_call_graph(const llvm::Module* module);
    LLVMAbstractState analyze_function_call(const llvm::CallInst* call, const LLVMAbstractState& caller_state, const AnalysisContext& context);
    void update_memory_model(const llvm::Instruction* inst, LLVMAbstractState& state) const;
    
    // Configuration options
    struct Config {
        bool enable_widening = true;
        bool enable_narrowing = true;
        size_t max_iterations = 1000;
        size_t widening_threshold = 5;
        bool track_memory = true;
        bool interprocedural = true;
        size_t max_call_depth = 10;
        bool context_sensitive = true;
        bool enable_pointer_analysis = true;
        bool enable_alias_analysis = true;
    } config;
};

} // namespace llvm_ai
} // namespace sparta
