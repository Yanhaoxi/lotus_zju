/**
 * Free Structure - DAG representation for symbolic expressions
 */

#ifndef FPSOLVE_FREE_STRUCTURE_H
#define FPSOLVE_FREE_STRUCTURE_H

#include "Solvers/FPSolve/DataStructs/Var.h"
#include <iostream>
#include <unordered_map>
#include <memory>
#include <utility>

namespace fpsolve {

class Node;
class Addition;
class Multiplication;
class Star;
class Element;
class Epsilon;
class Empty;

typedef const Node* NodePtr;

class NodeVisitor;
class NodeFactory;

// Base node class
class Node {
public:
  virtual ~Node() = default;
  virtual void Accept(NodeVisitor &visitor) const = 0;
};

std::string NodeToRawString(const Node &node);
std::string NodeToString(const Node &node);
std::ostream& operator<<(std::ostream &out, const Node &node);

// Visitor pattern for traversing nodes
class NodeVisitor {
public:
  virtual ~NodeVisitor() = default;
  virtual void Visit(const Addition &a);
  virtual void Visit(const Multiplication &m);
  virtual void Visit(const Star &s);
  virtual void Visit(const Element &e);
  virtual void Visit(const Epsilon &e);
  virtual void Visit(const Empty &e);
};

// Addition node: lhs + rhs
class Addition : public Node {
public:
  ~Addition() = default;
  void Accept(NodeVisitor &visitor) const override { visitor.Visit(*this); }
  NodePtr GetLhs() const { return lhs; }
  NodePtr GetRhs() const { return rhs; }

private:
  Addition(NodePtr l, NodePtr r) : lhs(l), rhs(r) {}
  const NodePtr lhs;
  const NodePtr rhs;
  friend class NodeFactory;
};

// Multiplication node: lhs * rhs
class Multiplication : public Node {
public:
  ~Multiplication() = default;
  void Accept(NodeVisitor &visitor) const override { visitor.Visit(*this); }
  NodePtr GetLhs() const { return lhs; }
  NodePtr GetRhs() const { return rhs; }

private:
  Multiplication(NodePtr l, NodePtr r) : lhs(l), rhs(r) {}
  const NodePtr lhs;
  const NodePtr rhs;
  friend class NodeFactory;
};

// Star node: node*
class Star : public Node {
public:
  ~Star() = default;
  void Accept(NodeVisitor &visitor) const override { visitor.Visit(*this); }
  NodePtr GetNode() const { return node; }

private:
  Star(NodePtr n) : node(n) {}
  const NodePtr node;
  friend class NodeFactory;
};

// Element node: variable
class Element : public Node {
public:
  ~Element() = default;
  void Accept(NodeVisitor &visitor) const override { visitor.Visit(*this); }
  VarId GetVar() const { return var; }

private:
  Element(VarId v) : var(v) {}
  const VarId var;
  friend class NodeFactory;
};

// Empty node: 0 (null element)
class Empty : public Node {
public:
  ~Empty() = default;
  void Accept(NodeVisitor &visitor) const override { visitor.Visit(*this); }
private:
  Empty() = default;
  friend class NodeFactory;
};

// Epsilon node: 1 (identity element)
class Epsilon : public Node {
public:
  ~Epsilon() = default;
  void Accept(NodeVisitor &visitor) const override { visitor.Visit(*this); }
private:
  Epsilon() = default;
  friend class NodeFactory;
};

// Hash function for pairs of NodePtr
struct NodePairHash {
  std::size_t operator()(const std::pair<NodePtr, NodePtr>& p) const {
    std::size_t h1 = std::hash<const void*>{}(p.first);
    std::size_t h2 = std::hash<const void*>{}(p.second);
    return h1 ^ (h2 << 1);
  }
};

// Factory for creating nodes with hash-consing
class NodeFactory {
public:
  NodeFactory() : empty_(new Empty), epsilon_(new Epsilon) {}
  
  virtual ~NodeFactory() {
    for (auto &pair : additions_) { delete pair.second; }
    for (auto &pair : multiplications_) { delete pair.second; }
    for (auto &pair : stars_) { delete pair.second; }
    for (auto &pair : elems_) { delete pair.second; }
    delete empty_;
    delete epsilon_;
  }

  virtual NodePtr NewAddition(NodePtr lhs, NodePtr rhs);
  virtual NodePtr NewMultiplication(NodePtr lhs, NodePtr rhs);
  virtual NodePtr NewStar(NodePtr node);
  virtual NodePtr NewElement(VarId var);
  virtual NodePtr GetEmpty() const { return empty_; }
  virtual NodePtr GetEpsilon() const { return epsilon_; }

  virtual void PrintDot(std::ostream &out);
  virtual void GC();
  virtual void PrintStats(std::ostream &out = std::cout);

private:
  std::unordered_map<std::pair<NodePtr, NodePtr>, NodePtr, NodePairHash> additions_;
  std::unordered_map<std::pair<NodePtr, NodePtr>, NodePtr, NodePairHash> multiplications_;
  std::unordered_map<NodePtr, NodePtr> stars_;
  std::unordered_map<VarId, NodePtr> elems_;
  NodePtr empty_;
  NodePtr epsilon_;
};

} // namespace fpsolve

#endif // FPSOLVE_FREE_STRUCTURE_H

