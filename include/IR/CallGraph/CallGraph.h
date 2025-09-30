/*
An interface for callgraph construction and analysis.

- Build callgraph: type-based, pointer analsyis-based, LLM-enhanced, etc.
- Visualzie and Export callgraph: DOT, JSON, etc.
- Query callgraph: callers/callees, reachability, call paths, SCC, code metrics, etc.
- Schedule using callgraph: bottom-up, top-down, etc.
- Transform callgraph: break cycles (remove back edges?), etc.
- Provide services: LSP server, MCP server, etc?
*/


#pragma once

#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"
#include <memory>
#include <vector>
#include <unordered_set>
#include <unordered_map>

namespace lotus {

// Forward declarations
class CallGraphNode;
class CallGraphBuilder;
class CallGraphAnalyzer;

/**
 * Core CallGraph interface - minimal, focused on essential operations
 */
class CallGraph {
public:
    using NodeId = size_t;
    using NodeSet = std::unordered_set<NodeId>;
    using EdgeMap = std::unordered_map<NodeId, NodeSet>;
    
    virtual ~CallGraph() = default;
    
    // Core graph operations
    virtual NodeId addNode(llvm::Function* func) = 0;
    virtual void addEdge(NodeId caller, NodeId callee) = 0;
    virtual bool hasNode(NodeId id) const = 0;
    virtual bool hasEdge(NodeId caller, NodeId callee) const = 0;
    
    // Node access
    virtual llvm::Function* getFunction(NodeId id) const = 0;
    virtual NodeId getNode(llvm::Function* func) const = 0;
    virtual const NodeSet& getCallees(NodeId caller) const = 0;
    virtual const NodeSet& getCallers(NodeId callee) const = 0;
    
    // Graph properties
    virtual size_t numNodes() const = 0;
    virtual size_t numEdges() const = 0;
    virtual bool isEmpty() const = 0;
    
    // Iteration
    virtual auto begin() const -> decltype(EdgeMap().begin()) = 0;
    virtual auto end() const -> decltype(EdgeMap().end()) = 0;
};

/**
 * CallGraphBuilder - handles construction from various sources
 */
class CallGraphBuilder {
public:
    virtual ~CallGraphBuilder() = default;
    
    // Construction methods
    virtual std::unique_ptr<CallGraph> buildFromModule(llvm::Module& M) = 0;
    virtual std::unique_ptr<CallGraph> buildFromPointerAnalysis(llvm::Module& M) = 0;
    virtual std::unique_ptr<CallGraph> buildFromTypeAnalysis(llvm::Module& M) = 0;
    
    // Configuration
    virtual void setContextSensitive(bool enabled) = 0;
    virtual void setIncludeExternalCalls(bool enabled) = 0;
    virtual void setResolveIndirectCalls(bool enabled) = 0;
};

/**
 * CallGraphAnalyzer - handles queries and analysis
 */
class CallGraphAnalyzer {
public:
    virtual ~CallGraphAnalyzer() = default;
    
    // Reachability queries
    virtual bool isReachable(CallGraph::NodeId from, CallGraph::NodeId to) const = 0;
    virtual std::vector<CallGraph::NodeId> getReachableNodes(CallGraph::NodeId from) const = 0;
    virtual std::vector<CallGraph::NodeId> getCallPath(CallGraph::NodeId from, CallGraph::NodeId to) const = 0;
    
    // SCC analysis
    virtual std::vector<std::vector<CallGraph::NodeId>> getSCCs() const = 0;
    virtual bool isInCycle(CallGraph::NodeId node) const = 0;
    
    // Metrics
    virtual size_t getInDegree(CallGraph::NodeId node) const = 0;
    virtual size_t getOutDegree(CallGraph::NodeId node) const = 0;
    virtual size_t getDepth(CallGraph::NodeId node) const = 0;
    
    // Topological operations
    virtual std::vector<CallGraph::NodeId> getTopologicalOrder() const = 0;
    virtual std::vector<CallGraph::NodeId> getBottomUpOrder() const = 0;
};

/**
 * CallGraphExporter - handles visualization and export
 */
class CallGraphExporter {
public:
    virtual ~CallGraphExporter() = default;
    
    // Export formats
    virtual std::string toDOT() const = 0;
    virtual std::string toJSON() const = 0;
    virtual std::string toGraphML() const = 0;
    
    // File export
    virtual void exportToFile(const std::string& filename, const std::string& format) const = 0;
};

/**
 * Factory for creating call graph components
 */
class CallGraphFactory {
public:
    static std::unique_ptr<CallGraphBuilder> createBuilder();
    static std::unique_ptr<CallGraphAnalyzer> createAnalyzer(const CallGraph& cg);
    static std::unique_ptr<CallGraphExporter> createExporter(const CallGraph& cg);
};

} // namespace lotus