#ifndef POINTER_ANALYSIS_H
#define POINTER_ANALYSIS_H

#include "llvm/IR/Value.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstVisitor.h"
#include <unordered_map>
#include <unordered_set>
#include <tuple>
#include <vector>
#include <utility>
#include <functional>
#include <deque>
#include "Alias/OriginAA/CallGraph.h"
#include "Alias/OriginAA/ChannelSemantics.h"
#include "Alias/OriginAA/Flags.h"

namespace std
{
    template <>
    struct hash<std::vector<uint64_t>>
    {
        std::size_t operator()(const std::vector<uint64_t> &v) const noexcept
        {
            std::size_t h = 0;
            for (auto x : v)
                h ^= std::hash<uint64_t>()(x) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };

    template <>
    struct hash<std::tuple<llvm::Value *, llvm::Context, std::vector<uint64_t>, bool>>
    {
        std::size_t operator()(const std::tuple<llvm::Value *, llvm::Context, std::vector<uint64_t>, bool> &t) const noexcept
        {
            std::size_t h1 = std::hash<llvm::Value *>()(std::get<0>(t));
            std::size_t h2 = std::hash<llvm::Context>()(std::get<1>(t));
            std::size_t h3 = std::hash<std::vector<uint64_t>>()(std::get<2>(t));
            std::size_t h4 = std::hash<bool>()(std::get<3>(t));
            return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
        }
    };
}

namespace llvm
{
    class Instruction;
    class Module;
    class StoreInst;
    class LoadInst;
    class BitCastInst;
    class GetElementPtrInst;
    class AllocaInst;

    // Forward declare ChannelSemantics
    class ChannelSemantics;

    struct Node
    {
        uint64_t id;                      // Unique node ID
        llvm::Value *value;               // The LLVM value
        Context context;                  // The context
        std::vector<uint64_t> offsets;    // For field-sensitive analysis, stores the offsets of the fields
        std::unordered_set<uint64_t> pts; // Points-to set (final)

        llvm::Type *type; // type of the value (or the type the pointer can hold), used for type checking

        // used during solving and propogating
        std::unordered_set<uint64_t> diff; // newly added nodes into points-to set; will be added to pts after propogation and reset for next iteration
        Node *alias = nullptr;             // Union-find for aliasing: used when actual parameter is used as return value with GEP, store and memcpy, only happens on the 1st param e.g.,
        // define internal void @"_ZN3std4sync4mpmc4list16Channel$LT$T$GT$3new17h9fbe3e677e1b4f13E"(ptr sret(%"std::sync::mpmc::list::Channel<i32>") %0) ...
        //   %7 = getelementptr inbounds %"std::sync::mpmc::list::Channel<i32>", ptr %0, i32 0, i32 2, !dbg !5323
        //   call void @llvm.memcpy.p0.p0.i64(ptr align 128 %7, ptr align 128 %_6, i64 128, i1 false), !dbg !5323

        Node *findAliasRoot()
        {
            if (!alias)
                return this;
            return alias = alias->findAliasRoot();
        }

        void unionAlias(Node *other)
        {
            Node *root1 = this->findAliasRoot();
            Node *root2 = other->findAliasRoot();
            if (root1 == root2)
                return; // Prevents cycles!
            root2->alias = root1;
            // Merge points-to sets as needed
        }

        // Constructor
        Node(int nodeId, llvm::Value *v, Context ctx = Everywhere(), std::vector<uint64_t> idx = {}) : id(nodeId), value(v), type(v ? v->getType() : nullptr), context(ctx), offsets(std::move(idx)) {}

        // Equality operator for unordered_map/unordered_set
        bool operator==(const Node &other) const
        {
            return value == other.value && context == other.context && id == other.id && offsets == other.offsets;
        }

        virtual bool isAlloc() const
        {
            return false; // Base Node is not an allocation node
        }

        virtual void print(llvm::raw_ostream &os) const
        {
            os << "[Node id=" << id << ", value=";
            if (value)
            {
                if (auto f = dyn_cast<Function>(value))
                    os << f->getName();
                else
                    value->print(os);
            }
            else
                os << "null";
            // os << ", type=";
            // if (type)
            //     type->print(os, false);
            // else
            //     os << "null";
            // os << ", ";
            if (auto *inst = llvm::dyn_cast<llvm::Instruction>(value))
            {
                llvm::Function *func = inst->getParent()->getParent();
                if (func)
                {
                    os << " (from function " << func->getName() << ")";
                }
            }
            else if (auto *arg = llvm::dyn_cast<llvm::Argument>(value))
            {
                if (auto *func = arg->getParent())
                {
                    os << " (arg of function " << func->getName() << ")";
                }
            }
            else if (auto *func = llvm::dyn_cast<llvm::Function>(value))
            {
                os << " (ret of function " << func->getName() << ")";
            }
            else
            {
                os << " (no function context)";
            }
            os << ", context=";
            os << "[";
            if (context == Everywhere())
            {
                os << "Everywhere";
            }
            else
            {

                for (auto it = context.begin(); it != context.end(); ++it)
                {
                    if (*it)
                        (*it)->print(os);
                    else
                        os << "null";
                    if (std::next(it) != context.end())
                        os << ", ";
                }
            }
            os << "]";
            if (!offsets.empty())
            {
                os << ", indices=["; // or fields
                for (size_t i = 0; i < offsets.size(); ++i)
                {
                    os << offsets[i];
                    if (i + 1 < offsets.size())
                        os << ",";
                }
                os << "]";
            }
            os << ", pts=[";
            if (pts.empty())
            {
                os << "empty";
            }
            else
            {
                for (auto it = pts.begin(); it != pts.end(); ++it)
                {
                    os << *it;
                    if (std::next(it) != pts.end())
                        os << ",";
                }
            }
            os << "]";
            if (!diff.empty())
            {
                os << ", diff=[";
                for (auto it = diff.begin(); it != diff.end(); ++it)
                {
                    os << *it;
                    if (std::next(it) != diff.end())
                        os << ",";
                }
                os << "]";
            }
            os << "]";
        }
    };

    struct AllocNode : public Node
    {
        llvm::Type *type; // allocated type, used for type checking

        // Constructor: call base Node constructor
        AllocNode(int nodeId, llvm::Value *v, Context ctx = Everywhere(), std::vector<uint64_t> idx = {})
            : Node(nodeId, v, ctx, std::move(idx))
        {
            if (auto *allocaInst = dyn_cast<AllocaInst>(v))
            {
                type = allocaInst->getAllocatedType();

                if (DebugMode)
                {
                    errs() << "AllocNode created with id=" << id << ", value=" << v << "\n";

                    if (auto *structType = dyn_cast<StructType>(type))
                    { // If it's a named struct
                        if (structType->hasName())
                        {
                            errs() << "Struct name: " << structType->getName() << "\n";

                            // inspect fields
                            for (unsigned i = 0; i < structType->getNumElements(); ++i)
                            {
                                Type *field = structType->getElementType(i);
                                errs() << "  Field " << i << ": ";
                                field->print(errs());
                                errs() << "\n";
                            }
                        }
                        else
                        { // is anonymous struct
                            errs() << "Anonymous struct with " << structType->getNumElements() << " elements:\n";
                            for (unsigned i = 0; i < structType->getNumElements(); ++i)
                            {
                                Type *field = structType->getElementType(i);
                                errs() << "  Field " << i << ": ";
                                field->print(errs());
                                errs() << "\n";
                            }
                        }
                    }
                    else if (auto *AT = dyn_cast<ArrayType>(type))
                    {
                        errs() << "Array of " << AT->getNumElements() << " elements of type: ";
                        AT->getElementType()->print(errs());
                        errs() << "\n";
                    }
                    else if (type->isPointerTy())
                    {
                        errs() << "Pointer to type: ";
                        type->print(errs());
                        errs() << "\n";
                    }
                    else if (type->isIntegerTy())
                    {
                        errs() << "Integer type: i" << cast<IntegerType>(type)->getBitWidth() << "\n";
                    }
                    else if (type->isFloatingPointTy())
                    {
                        errs() << "Floating point type";
                    }
                    else
                    {
                        errs() << "Other type: ";
                        type->print(errs());
                        errs() << "\n";
                    }
                }
            }
        }

        bool isAlloc() const override
        {
            return true; // AllocNode is always an allocation node
        }

        // You can override print to include alloc info
        void print(llvm::raw_ostream &os) const override
        {
            os << "[AllocNode id=" << id << ", value=";
            if (value)
            {
                if (auto f = dyn_cast<Function>(value))
                    os << f->getName();
                else
                    value->print(os);
            }
            else
                os << "null";
            // os << ", type=";
            // if (type)
            //     type->print(os, false);
            // else
            //     os << "null";
            // os << ", ";
            if (auto *inst = llvm::dyn_cast<llvm::Instruction>(value))
            {
                llvm::Function *func = inst->getParent()->getParent();
                if (func)
                {
                    os << " (from function " << func->getName() << ")";
                }
            }
            // else if (auto *arg = llvm::dyn_cast<llvm::Argument>(value))
            // {
            //     if (auto *func = arg->getParent())
            //     {
            //         os << " (arg of function " << func->getName() << ")";
            //     }
            // }
            // else if (auto *func = llvm::dyn_cast<llvm::Function>(value))
            // {
            //     os << " (ret of function " << func->getName() << ")";
            // }
            else
            {
                os << " (no function context)";
            }
            os << ", context=";
            os << "[";
            if (context == Everywhere())
            {
                os << "Everywhere";
            }
            else
            {

                for (auto it = context.begin(); it != context.end(); ++it)
                {
                    if (*it)
                        (*it)->print(os);
                    else
                        os << "null";
                    if (std::next(it) != context.end())
                        os << ", ";
                }
            }
            os << "]";
            if (!offsets.empty())
            {
                os << ", indices=["; // or fields
                for (size_t i = 0; i < offsets.size(); ++i)
                {
                    os << offsets[i];
                    if (i + 1 < offsets.size())
                        os << ",";
                }
                os << "]";
            }
        }
    };

    // Overload operator<< for Node as a free function
    inline llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const llvm::Node &node)
    {
        node.print(os);
        return os;
    }

    // Overload operator<< for AllocNode as a free function
    inline llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const llvm::AllocNode &node)
    {
        node.print(os);
        return os;
    }

    enum ConstraintType
    {
        Assign,    // copy
        AddressOf, // address of, e.g., %b = &%a
        Offset,    // offset, e.g., %b = getelementptr %a, 0, 1
        Load,
        Store,
        Invoke, // dynamic dispatch
    };

    struct Constraint // use UINT64_MAX for null
    {
        ConstraintType type;
        uint64_t lhs_id; // Source/LHS Node ID
        uint64_t rhs_id; // Destination/RHS Node ID

        std::vector<uint64_t> offsets; // For field-sensitive analysis, field offsets

        Constraint(ConstraintType t, uint64_t s, uint64_t d, std::vector<uint64_t> idx = {})
            : type(t), lhs_id(s), rhs_id(d), offsets(std::move(idx)) {}

        // Equality operator for unordered_map/unordered_set
        bool operator==(const Constraint &other) const
        {
            return type == other.type && lhs_id == other.lhs_id && rhs_id == other.rhs_id && offsets == other.offsets;
        }

        void print(llvm::raw_ostream &os) const
        {
            const char *typeStr = nullptr;
            switch (type)
            {
            case Assign:
                typeStr = "Assign";
                break;
            case AddressOf:
                typeStr = "AddressOf";
                break;
            case Offset:
                typeStr = "Offset";
                break;
            case Load:
                typeStr = "Load";
                break;
            case Store:
                typeStr = "Store";
                break;
            case Invoke:
                typeStr = "Invoke";
                break;
            default:
                typeStr = "Unknown";
                break;
            }
            os << "\t" << typeStr
               << " src=";
            if (lhs_id != UINT64_MAX)
            {
                os << lhs_id;
            }
            else
            {
                os << "null";
            }
            os << " dst=";
            if (rhs_id != UINT64_MAX)
            {
                os << rhs_id;
            }
            else
            {
                os << "null";
            }
            if (!offsets.empty())
            {
                os << ", offsets=[";
                for (size_t i = 0; i < offsets.size(); ++i)
                {
                    os << offsets[i];
                    if (i + 1 < offsets.size())
                        os << ",";
                }
                os << "]";
            }
        }
    };

    // Overload operator<< for Node as a free function
    inline llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const llvm::Constraint &c)
    {
        c.print(os);
        return os;
    }

    // PointerAnalysis class: performs pointer analysis on LLVM IR
    class PointerAnalysis : public InstVisitor<PointerAnalysis>
    {
    public:
        // settings
        bool DebugMode = false;          // Debug flag to enable or disable debugging output
        int MaxVisit = 2;                // Maximum number of times a CGNode can be visited
        bool HandleIndirectCalls = true; // Whether to handle indirect calls
        bool TaintingEnabled = false;    // Enable tainting analysis
        Module &M;

        PointerAnalysis(Module &M) : M(M)
        {
            if (DebugMode)
                llvm::errs() << "PointerAnalysis initialized with module: " << M.getName() << "\n";
        }

        void analyze();
        const CallGraph &getCallGraph() const { return callGraph; }
        const std::unordered_set<Function *> &getVisitedFunctions() const
        {
            return Visited;
        }
        const std::string getOutputFileName() const { return outputFile; }

        const void printStatistics();
        void printPointsToMap(std::ofstream &os) const;
        void printCallGraph(std::ofstream &os) const;
        void printTaintedNodes(std::ofstream &os);
        
        // Configuration methods
        void setDebugMode(bool debug) { DebugMode = debug; }
        void setTaintingEnabled(bool enabled) { TaintingEnabled = enabled; }
        void setHandleIndirectCalls(bool enabled) { HandleIndirectCalls = enabled; }
        void setMaxVisit(unsigned max) { MaxVisit = max; }

        void clear()
        {
            idToNodeMap.clear();
            Visited.clear();
            Worklist.clear();
            FunctionWorklist.clear();
            vtableToFunctionMap.clear();
            ValueContextToNodeMap.clear();
            callGraph.clear();
        }

        std::vector<Constraint> Worklist; // Worklist for new constraints to visit

        Node *getOrCreateNode(llvm::Value *value, Context context = Everywhere(), std::vector<uint64_t> indices = {}, bool isAlloc = false); // create or find node: ctx == Everywhere
        Node *getNodebyID(uint64_t id);                                                                                                    // Get node by ID
        virtual Context getContext(Context context = Everywhere(), const Value *newCallSite = nullptr);
        virtual void processInstruction(Instruction &I, CGNode *cgnode);

        // Visitor methods
        void visitStoreInst(StoreInst &I);
        void visitLoadInst(LoadInst &I);
        virtual void visitAllocaInst(AllocaInst &I);
        void visitBitCastInst(BitCastInst &I);
        void visitUnaryOperator(UnaryOperator &UO);
        void visitGetElementPtrInst(GetElementPtrInst &I);
        void visitExtractValueInst(ExtractValueInst &EVI);
        void visitPHINode(PHINode &I);
        void visitAtomicRMWInst(AtomicRMWInst &I);
        void visitAtomicCmpXchgInst(AtomicCmpXchgInst &I);
        virtual void visitInvokeInst(InvokeInst &I);
        virtual void visitCallInst(CallInst &I);
        void visitReturnInst(ReturnInst &I);
        void addConstraintForCall(CallBase &CB, Function *F); // Add constraints for call instructions, including parameters and return value
        void visitInstruction(Instruction &I);                // fallback

        void processAssignConstraint(const llvm::Constraint &constraint);
        void processAddressOfConstraint(const llvm::Constraint &constraint);
        void processGEPConstraint(const llvm::Constraint &constraint); // Process GEP constraints
        void processLoadConstraint(const llvm::Constraint &constraint);
        void processStoreConstraint(const llvm::Constraint &constraint);
        void processInvokeConstraints(const llvm::Constraint &constraint);                             // Process constraints for indirect invoke instructions
        bool handleRustTry(CallBase &CB, Function *F);                                                 // handle __rust_try
        void handleSpecialDeclaredFunction(CallBase &CI, Function *F, CGNode realCaller = NullCGNode()); // Handle certain declared functions: call invoked through vtable needs realCaller

        ChannelSemantics *channelSemantics; // Channel semantics integration
        void setChannelSemantics(ChannelSemantics *cs) { channelSemantics = cs; }

    protected:
        uint64_t nextNodeId = 0; // Monotonically increasing node ID
        llvm::Function *mainFn;  // Real main function, not the one "main" for rust

        std::unordered_map<uint64_t, Node *> idToNodeMap; // Map from node ID to Node
        llvm::CallGraph callGraph;                        // Call graph to track caller-callee relationships
        std::unordered_set<Function *> Visited;           // visited functions
        std::unordered_map<CGNode, int> VisitCount;       // Track the number of visits for each function/cgnode
        std::vector<CGNode> FunctionWorklist;             // Worklist for new functions (with context) to visit

        std::unordered_map<std::tuple<llvm::Value *, Context, std::vector<uint64_t>, bool>, Node *> ValueContextToNodeMap; // Map to track Value and context pairs to Node
        std::unordered_map<ConstantAggregate *, std::vector<Function *>> vtableToFunctionMap;                              // Map to track vtable to function mappings

        std::vector<llvm::Function *> getVtable(GlobalVariable *GV); // compute vtable's functions and store to vtableToFunctionMap
        bool excludeFunctionFromAnalysis(Function *F);               // Exclude certain functions from analysis, e.g., llvm.dbg.declare
        void AddToFunctionWorklist(CGNode *callee);
        virtual void processGlobalVar(GlobalVariable &GV);
        void visitFunction(CGNode *cgnode);
        bool useParamAsReturnValue(Argument *param); // Check if the first parameter is used as a return value

        // used to track the current context and CGNode during analysis
        CGNode *CurrentCGNode = nullptr;
        Context CurrentContext;

        std::unordered_map<uint64_t, std::vector<Constraint>> DU; // def-use constraints
        void addConstraint(const Constraint &constraint);         // Add a constraint to the worklist and update def-use map
        void sortConstraints();
        void solveConstraints();
        void propagateDiff(uint64_t id); // Propagate the diff set: for all constraints that use dst, push them to Worklist

        std::string inputDir;           // Directory containing the JSON file
        std::string outputFile;         // Output file name
        bool parseInputDir(Module &M);  // Parse the input directory from the module
        bool parseOutputDir(Module &M); // Parse the output file path from the module

        llvm::Function *parseMainFn(Module &M); // Parse the main function from the module
        void onthefly(Module &M);               // On-the-fly analysis

        bool isTypeCompatible(Type *ptrType, Type *allocaType); // Check if the pointer type is compatible with the alloca type

        // Channel-specific analysis methods
        bool handleChannelConstraints();

        // the following for taint analysis
        struct FnSignature
        {
            std::string fn_name;           // package name::function name
            std::vector<std::string> args; // argument types -> impossible to match due to pointer and compiler optimizations
            std::string returnType;        // return type
        };

        std::string taintJsonFile;                             // JSON file name
        std::unordered_set<FnSignature *> TaintedFnSignatures; // tainted function signatures from JSON
        std::unordered_set<uint64_t> TaintedNodeIDs;           // node ids that are tainted
        bool parseTaintConfig(Module &M);
        bool isTaintedFunction(const CallBase &callsite);

        // the following for handling tokio tasks with less constraints
        std::unordered_map<std::string, llvm::Node *> fnName2TaskNodeMap; // Map from function name to task node, e.g., _ZN4demo16spawn_user_query17he2469db56cab90c3E -> sret(%"[async fn body@src/main.rs:14:3: 36:2]") %0 in examples/tokio-demo/src/main.rs
        bool handleTokioTask(CallBase &CB, Function *calledFn);
        bool handleTokioRawVtable(CallBase &CB, Function *F); // handle tokio raw vtable functions
    };

} // namespace llvm

#endif