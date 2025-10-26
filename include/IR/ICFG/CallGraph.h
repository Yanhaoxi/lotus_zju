#ifndef LT_CALL_GRAPH_H
#define LT_CALL_GRAPH_H
 
#include <map>
//#include <set>
//#include <list>
//#include <stack>
 
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Instructions.h>
 

 
 class LTCallGraphNode;
 
 /// The basic data container for the call graph of a \c llvm::Module of IR.
 ///
 /// This class exposes both the interface to the call graph for a module of IR.
 ///
 /// The core call graph itself can also be updated to reflect changes to the IR.
 class LTCallGraph {
     llvm::Module &M;
 
     using FunctionMapTy =
     std::map<const llvm::Function *, std::unique_ptr<LTCallGraphNode>>;
 
     /// A map from \c llvm::Function* to \c LTCallGraphNode*.
     FunctionMapTy FunctionMap;
 
     /// This node has edges to all external functions and those internal
     /// functions that have their address taken.
     LTCallGraphNode *ExternalCallingNode = nullptr;
 
     /// This node has edges to it from all functions making indirect calls
     /// or calling an external function.
     std::unique_ptr<LTCallGraphNode> CallsExternalNode = nullptr;
 
     /// Replace the function represented by this node by another.
     ///
     /// This does not rescan the body of the function, so it is suitable when
     /// splicing the body of one function to another while also updating all
     /// callers from the old function to the new.
     void spliceFunction(const llvm::Function *From, const llvm::Function *To);
 
     /// Add a function to the call graph, and link the node to all of the
     /// functions that it calls.
     void addToCallGraph(llvm::Function *F);
 
 public:
     explicit LTCallGraph(llvm::Module &M);
     LTCallGraph(LTCallGraph &&Arg);
     ~LTCallGraph();
 
     /// Print out this call graph node.
     void dump() const;
 
     using iterator = FunctionMapTy::iterator;
     using const_iterator = FunctionMapTy::const_iterator;
 
     /// Returns the module the call graph corresponds to.
     llvm::Module &getModule() const { return M; }
 
     inline iterator begin() { return FunctionMap.begin(); }
     inline iterator end() { return FunctionMap.end(); }
     inline const_iterator begin() const { return FunctionMap.begin(); }
     inline const_iterator end() const { return FunctionMap.end(); }
 
     /// Returns the call graph node for the provided function.
     inline const LTCallGraphNode *operator[](const llvm::Function *F) const {
         const_iterator I = FunctionMap.find(F);
         assert(I != FunctionMap.end() && "llvm::Function not in callgraph!");
         return I->second.get();
     }
 
     /// Returns the call graph node for the provided function.
     inline LTCallGraphNode *operator[](const llvm::Function *F) {
         const_iterator I = FunctionMap.find(F);
         assert(I != FunctionMap.end() && "llvm::Function not in callgraph!");
         return I->second.get();
     }
 
     /// Returns the \c LTCallGraphNode which is used to represent
     /// undetermined calls into the callgraph.
     LTCallGraphNode *getExternalCallingNode() const { return ExternalCallingNode; }
 
     LTCallGraphNode *getCallsExternalNode() const {
         return CallsExternalNode.get();
     }
 
     //===---------------------------------------------------------------------
     // Functions to keep a call graph up to date with a function that has been
     // modified.
     //
 
     /// Unlink the function from this module, returning it.
     ///
     /// Because this removes the function from the module, the call graph node is
     /// destroyed.  This is only valid if the function does not call any other
     /// functions (ie, there are no edges in it's CGN).  The easiest way to do
     /// this is to dropAllReferences before calling this.
     llvm::Function *removeFunctionFromModule(LTCallGraphNode *CGN);
 
     /// Similar to operator[], but this will insert a new LTCallGraphNode for
     /// \c F if one does not already exist.
     LTCallGraphNode *getOrInsertFunction(const llvm::Function *F);
 };
 
 /// A node in the call graph for a module.
 ///
 /// Typically represents a function in the call graph. There are also special
 /// "null" nodes used to represent theoretical entries in the call graph.
 class LTCallGraphNode {
 public:
     /// A pair of the calling instruction (a call or invoke)
     /// and the call graph node being called.
     using CallRecord = std::pair<llvm::Value*, LTCallGraphNode*>;
 
 public:
     using CalledFunctionsVector = std::vector<CallRecord>;
 
     /// Creates a node for the specified function.
     inline LTCallGraphNode(llvm::Function *F) : F(F) {}
 
     LTCallGraphNode(const LTCallGraphNode &) = delete;
     LTCallGraphNode &operator=(const LTCallGraphNode &) = delete;
 
     ~LTCallGraphNode() {
         assert(NumReferences == 0 && "Node deleted while references remain");
     }
 
     using iterator = std::vector<CallRecord>::iterator;
     using const_iterator = std::vector<CallRecord>::const_iterator;
 
     /// Returns the function that this call graph node represents.
     llvm::Function *getFunction() const { return F; }
 
     inline iterator begin() { return CalledFunctions.begin(); }
     inline iterator end() { return CalledFunctions.end(); }
     inline const_iterator begin() const { return CalledFunctions.begin(); }
     inline const_iterator end() const { return CalledFunctions.end(); }
     inline bool empty() const { return CalledFunctions.empty(); }
     inline unsigned size() const { return (unsigned)CalledFunctions.size(); }
 
     /// Returns the number of other CallGraphNodes in this LTCallGraph that
     /// reference this node in their callee list.
     unsigned getNumReferences() const { return NumReferences; }
 
     /// Returns the i'th called function.
     LTCallGraphNode *operator[](unsigned i) const {
         assert(i < CalledFunctions.size() && "Invalid index");
         return CalledFunctions[i].second;
     }
 
     //===---------------------------------------------------------------------
     // Methods to keep a call graph up to date with a function that has been
     // modified
     //
 
     /// Removes all edges from this LTCallGraphNode to any functions it
     /// calls.
     void removeAllCalledFunctions() {
         while (!CalledFunctions.empty()) {
             CalledFunctions.back().second->DropRef();
             CalledFunctions.pop_back();
         }
     }
 
     /// Moves all the callee information from N to this node.
     void stealCalledFunctionsFrom(LTCallGraphNode *N) {
         assert(CalledFunctions.empty() &&
                "Cannot steal callsite information if I already have some");
         std::swap(CalledFunctions, N->CalledFunctions);
     }
 
    /// Adds a function to the list of functions called by this one.
    void addCalledFunction(const llvm::Instruction *CS, LTCallGraphNode *M) {
        if (const auto *CB = llvm::dyn_cast_or_null<llvm::CallBase>(CS)) {
            assert(!CB->getCalledFunction() || !CB->getCalledFunction()->isIntrinsic());
        }
        CalledFunctions.emplace_back(const_cast<llvm::Value*>(static_cast<const llvm::Value*>(CS)), M);
        M->AddRef();
    }
 
     void removeCallEdge(iterator I) {
         I->second->DropRef();
         *I = CalledFunctions.back();
         CalledFunctions.pop_back();
     }
 
    /// Removes the edge in the node for the specified call site.
    ///
    /// Note that this method takes linear time, so it should be used sparingly.
    void removeCallEdgeFor(const llvm::Instruction *CS);
 
     /// Removes all call edges from this node to the specified callee
     /// function.
     ///
     /// This takes more time to execute than removeCallEdgeTo, so it should not
     /// be used unless necessary.
     void removeAnyCallEdgeTo(LTCallGraphNode *Callee);
 
     /// Removes one edge associated with a null callsite from this node to
     /// the specified callee function.
     void removeOneAbstractEdgeTo(LTCallGraphNode *Callee);
 
    /// Replaces the edge in the node for the specified call site with a
    /// new one.
    ///
    /// Note that this method takes linear time, so it should be used sparingly.
    void replaceCallEdge(const llvm::Instruction *CS, const llvm::Instruction *NewCS, LTCallGraphNode *NewNode);
 
     void dump() const;
 
 private:
     friend class LTCallGraph;
 
     // we should initialize it
     llvm::Function *F = nullptr;
 
     std::vector<CallRecord> CalledFunctions;
 
     /// The number of times that this LTCallGraphNode occurs in the
     /// CalledFunctions array of this or other CallGraphNodes.
     unsigned NumReferences = 0;
 
     void DropRef() { --NumReferences; }
     void AddRef() { ++NumReferences; }
 
     /// A special function that should only be used by the LTCallGraph class.
     void allReferencesDropped() { NumReferences = 0; }
 };

 
 #endif //LT_CALL_GRAPH_H