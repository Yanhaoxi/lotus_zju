#ifndef ANDERSEN_NODE_FACTORY_H
#define ANDERSEN_NODE_FACTORY_H

#include <llvm/ADT/DenseMap.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Value.h>

#include <vector>

// AndersNode class - This class is used to represent a node in the constraint
// graph.  Due to various optimizations, it is not always the case that there is
// always a mapping from a Node to a Value. (In particular, we add artificial
// Node's that represent the set of pointed-to variables shared for each
// location equivalent Node. Ordinary clients are not allowed to create
// AndersNode objects. To guarantee index consistency, AndersNodes (and its
// subclasses) instances should only be created through AndersNodeFactory.
using NodeIndex = unsigned;
class AndersNode {
public:
  enum AndersNodeType { VALUE_NODE, OBJ_NODE };

private:
  AndersNodeType type;
  NodeIndex idx, mergeTarget;
  const llvm::Value *value;
  AndersNode(AndersNodeType t, unsigned i, const llvm::Value *v = nullptr)
      : type(t), idx(i), mergeTarget(i), value(v) {}

public:
  NodeIndex getIndex() const { return idx; }
  const llvm::Value *getValue() const { return value; }

  friend class AndersNodeFactory;
};

// This is the factory class of AndersNode
// It uses vectors to hold all Nodes in the program. Nodes are keyed by both a
// context token (opaque pointer) and an LLVM value to support context-sensitive
// variants without changing the node indexing scheme.
class AndersNodeFactory {
public:
  // The largest unsigned int is reserved for invalid index
  static const unsigned InvalidIndex;

  using CtxKey = const void *;

private:
  // The set of nodes
  std::vector<AndersNode> nodes;

  // Some special indices
  static const NodeIndex UniversalPtrIndex = 0;
  static const NodeIndex UniversalObjIndex = 1;
  static const NodeIndex NullPtrIndex = 2;
  static const NodeIndex NullObjectIndex = 3;

  // Per-context node maps
  using ValueNodeMap = llvm::DenseMap<const llvm::Value *, NodeIndex>;
  llvm::DenseMap<CtxKey, ValueNodeMap> valueNodeMap;
  llvm::DenseMap<CtxKey, ValueNodeMap> objNodeMap;
  llvm::DenseMap<CtxKey, llvm::DenseMap<const llvm::Function *, NodeIndex>>
      returnMap;
  llvm::DenseMap<CtxKey, llvm::DenseMap<const llvm::Function *, NodeIndex>>
      varargMap;

  // Reverse lookup to gather nodes across contexts for a given value
  llvm::DenseMap<const llvm::Value *, std::vector<NodeIndex>> valueNodeBuckets;

public:
  AndersNodeFactory();

  // Factory methods (context-aware)
  NodeIndex createValueNode(const llvm::Value *val, CtxKey ctx);
  NodeIndex createObjectNode(const llvm::Value *val, CtxKey ctx);
  NodeIndex createReturnNode(const llvm::Function *f, CtxKey ctx);
  NodeIndex createVarargNode(const llvm::Function *f, CtxKey ctx);

  // Map lookup interfaces (return InvalidIndex if value not found)
  NodeIndex getValueNodeFor(const llvm::Value *val, CtxKey ctx) const;
  NodeIndex getValueNodeForConstant(const llvm::Constant *c,
                                    CtxKey ctx) const;
  NodeIndex getObjectNodeFor(const llvm::Value *val, CtxKey ctx) const;
  NodeIndex getObjectNodeForConstant(const llvm::Constant *c,
                                     CtxKey ctx) const;
  NodeIndex getReturnNodeFor(const llvm::Function *f, CtxKey ctx) const;
  NodeIndex getVarargNodeFor(const llvm::Function *f, CtxKey ctx) const;

  // Query all value nodes across contexts
  void getValueNodesFor(const llvm::Value *val,
                        std::vector<NodeIndex> &out) const;

  // Node merge interfaces
  void mergeNode(NodeIndex n0, NodeIndex n1); // Merge n1 into n0
  NodeIndex getMergeTarget(NodeIndex n);
  NodeIndex getMergeTarget(NodeIndex n) const;

  // Pointer arithmetic
  bool isObjectNode(NodeIndex i) const {
    return (nodes.at(i).type == AndersNode::OBJ_NODE);
  }
  NodeIndex getOffsetObjectNode(NodeIndex n, unsigned offset) const {
    assert(isObjectNode(n + offset));
    return n + offset;
  }

  // Special node getters
  NodeIndex getUniversalPtrNode() const { return UniversalPtrIndex; }
  NodeIndex getUniversalObjNode() const { return UniversalObjIndex; }
  NodeIndex getNullPtrNode() const { return NullPtrIndex; }
  NodeIndex getNullObjectNode() const { return NullObjectIndex; }

  // Value getters
  const llvm::Value *getValueForNode(NodeIndex i) const {
    return nodes.at(i).getValue();
  }
  void getAllocSites(std::vector<const llvm::Value *> &) const;

  // Value remover
  void removeNodeForValue(const llvm::Value *val) { valueNodeBuckets.erase(val); }

  // Size getters
  unsigned getNumNodes() const { return nodes.size(); }

  // For debugging purpose
  void dumpNode(NodeIndex) const;
  void dumpNodeInfo() const;
  void dumpRepInfo() const;
};

#endif
