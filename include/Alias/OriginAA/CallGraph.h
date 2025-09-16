
#pragma once
#include "llvm/IR/Function.h"
#include <deque>
#include <unordered_set>
#include <unordered_map>
#include <utility>

namespace llvm
{
    struct Context
    {
        std::deque<const llvm::Value *> values; // e.g., function or instruction

        Context() = default;
        Context(std::initializer_list<const llvm::Value *> il) : values(il) {}

        bool operator==(const Context &other) const
        {
            return values == other.values;
        }

        bool empty() const { return values.empty(); }
        size_t size() const { return values.size(); }
        const llvm::Value *operator[](size_t idx) const { return values[idx]; }
        auto begin() const { return values.begin(); }
        auto end() const { return values.end(); }

        void print(llvm::raw_ostream &os) const
        {
            os << "[";
            for (size_t i = 0; i < values.size(); ++i)
            {
                os << "(" + std::to_string(i) + ") ";
                auto ctx = values[i];
                if (ctx)
                    ctx->print(os);
                else
                    os << "null";
                if (i < values.size() - 1)
                    os << ", ";
            }
            os << "]";
        }
    };

    // Singleton instance for context-insensitive analysis
    inline const Context& Everywhere() {
        static const Context instance = Context();
        return instance;
    }

    // Overload operator<< for Node as a free function
    inline llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const llvm::Context &context)
    {
        context.print(os);
        return os;
    }

    struct CGNode
    {
        int id; // Unique node ID
        llvm::Function *function;
        Context context;

        // Default constructor
        CGNode() : id(0), function(nullptr), context(Everywhere()) {}

        CGNode(int id, llvm::Function *func, Context ctx = Everywhere())
            : id(id), function(func), context(ctx) {}

        bool operator==(const CGNode &other) const
        {
            if (function != other.function)
                return false;
            if (context == Everywhere() && other.context == Everywhere())
                return true;
            if (context.size() != other.context.size())
                return false;
            for (size_t i = 0; i < context.size(); ++i)
            {
                if (context[i] != other.context[i])
                    return false;
            }
            return true;
        }

        void print(llvm::raw_ostream &os) const
        {
            os << "[CGNode id=" << id << ", function=";
            if (function)
                os << function->getName();
            else
                os << "null";
            os << ", context=[";
            if (context == Everywhere())
            {
                os << "Everywhere";
            }
            else
            {
                context.print(os);
            }
            os << "]";
            os << "]";
        }
    };
    // Singleton instance for empty CGNode
    inline const CGNode& NullCGNode() {
        static const CGNode instance = CGNode();
        return instance;
    }

    // Overload operator<< for Node as a free function
    inline llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const llvm::CGNode &node)
    {
        node.print(os);
        return os;
    }

} // namespace llvm

namespace std
{
    template <>
    struct hash<llvm::Context>
    {
        std::size_t operator()(const llvm::Context &ctx) const noexcept
        {
            std::size_t h = 0;
            for (const auto *v : ctx.values)
            {
                h ^= std::hash<const llvm::Value *>{}(v) + 0x9e3779b9 + (h << 6) + (h >> 2);
            }
            return h;
        }
    };

    template <>
    struct hash<llvm::CGNode>
    {
        std::size_t operator()(const llvm::CGNode &node) const noexcept
        {
            std::size_t h1 = std::hash<llvm::Function *>{}(node.function);
            std::size_t h2 = std::hash<llvm::Context>{}(node.context);
            return h1 ^ (h2 << 1);
        }
    };

    template <>
    struct hash<std::pair<llvm::Value *, llvm::Context>>
    {
        std::size_t operator()(const std::pair<llvm::Value *, llvm::Context> &p) const noexcept
        {
            std::size_t h1 = std::hash<llvm::Value *>{}(p.first);
            std::size_t h2 = std::hash<llvm::Context>{}(p.second);
            return h1 ^ (h2 << 1);
        }
    };
}

namespace llvm
{
    class CallGraph
    {
    private:
        using NodeKey = std::pair<llvm::Function *, Context>;
        struct NodeKeyHash
        {
            std::size_t operator()(const NodeKey &k) const noexcept
            {
                std::size_t h1 = std::hash<llvm::Function *>{}(k.first);
                std::size_t h2 = 0;
                if (!k.second.empty())
                {
                    for (const auto *v : k.second)
                    {
                        h2 ^= std::hash<const llvm::Value *>{}(v) + 0x9e3779b9 + (h2 << 6) + (h2 >> 2);
                    }
                }
                // If context is empty, h2 remains 0
                return h1 ^ (h2 << 1);
            }
        };

        int nextNodeId = 0;                          // Monotonically increasing node ID
        std::unordered_map<int, CGNode> idToNodeMap; // Map from node ID to CGNode
        std::unordered_map<NodeKey, CGNode, NodeKeyHash> ValueContextToNodeMap;

    public:
        using CallEdgeSet = std::unordered_set<int>;
        using Node2EdgeSet = std::unordered_map<int, CallEdgeSet>;

        CGNode getOrCreateNode(llvm::Function *func, Context ctx = Everywhere());

        // Add an edge from caller to callee
        void addEdge(CGNode caller, CGNode callee)
        {
            graph_[caller.id].insert(callee.id);
        }

        // Get callees for a given caller
        const CallEdgeSet &getCallees(CGNode caller) const
        {
            static CallEdgeSet empty;
            auto it = graph_.find(caller.id);
            return it != graph_.end() ? it->second : empty;
        }

        const CGNode getNode(int id) const
        {
            auto it = idToNodeMap.find(id);
            return it != idToNodeMap.end() ? it->second : NullCGNode();
        }

        const CGNode getNode(llvm::Function *func, Context ctx = Everywhere()) const
        {
            auto it = ValueContextToNodeMap.find(std::make_pair(func, ctx));
            return it != ValueContextToNodeMap.end() ? it->second : NullCGNode();
        }

        // Get the underlying map (const)
        const Node2EdgeSet &getGraph() const { return graph_; }

        // Get the number of nodes
        size_t numNodes() const { return graph_.size(); }

        // Get the number of edges
        size_t numEdges() const
        {
            size_t edges = 0;
            for (const auto &entry : graph_)
                edges += entry.second.size();
            return edges;
        }

        // Iterate over all nodes
        auto begin() const { return graph_.begin(); }
        auto end() const { return graph_.end(); }

        void printCG(std::ofstream &outFile) const;

        void clear()
        {
            graph_.clear();
            idToNodeMap.clear();
            ValueContextToNodeMap.clear();
        }

    private:
        Node2EdgeSet graph_;
    };

} // namespace llvm
