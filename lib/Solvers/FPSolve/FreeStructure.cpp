/**
 * Free Structure implementation
 */

#include "Solvers/FPSolve/DataStructs/FreeStructure.h"
#include <sstream>
#include <set>

namespace fpsolve {

// Visitor default implementations
void NodeVisitor::Visit(const Addition &a) {
  a.GetLhs()->Accept(*this);
  a.GetRhs()->Accept(*this);
}

void NodeVisitor::Visit(const Multiplication &m) {
  m.GetLhs()->Accept(*this);
  m.GetRhs()->Accept(*this);
}

void NodeVisitor::Visit(const Star &s) {
  s.GetNode()->Accept(*this);
}

void NodeVisitor::Visit(const Element &e) {}
void NodeVisitor::Visit(const Epsilon &e) {}
void NodeVisitor::Visit(const Empty &e) {}

// String printer visitor
class StringPrinter : public NodeVisitor {
public:
  StringPrinter() : result_() {}
  
  void Visit(const Addition &a) override {
    result_ << "(";
    a.GetLhs()->Accept(*this);
    result_ << " + ";
    a.GetRhs()->Accept(*this);
    result_ << ")";
  }

  void Visit(const Multiplication &m) override {
    result_ << "(";
    m.GetLhs()->Accept(*this);
    result_ << " * ";
    m.GetRhs()->Accept(*this);
    result_ << ")";
  }

  void Visit(const Star &s) override {
    result_ << "(";
    s.GetNode()->Accept(*this);
    result_ << ")*";
  }

  void Visit(const Element &e) override {
    result_ << Var::GetVar(e.GetVar()).string();
  }

  void Visit(const Epsilon &e) override {
    result_ << "1";
  }

  void Visit(const Empty &e) override {
    result_ << "0";
  }

  std::string GetResult() const { return result_.str(); }

private:
  std::stringstream result_;
};

std::string NodeToString(const Node &node) {
  StringPrinter printer;
  node.Accept(printer);
  return printer.GetResult();
}

std::string NodeToRawString(const Node &node) {
  return NodeToString(node);
}

std::ostream& operator<<(std::ostream &out, const Node &node) {
  return out << NodeToString(node);
}

// NodeFactory implementations
NodePtr NodeFactory::NewAddition(NodePtr lhs, NodePtr rhs) {
  // Optimizations
  if (lhs == empty_) return rhs;
  if (rhs == empty_) return lhs;
  if (lhs == rhs) return lhs; // Idempotent

  auto key = std::make_pair(lhs, rhs);
  auto iter = additions_.find(key);
  if (iter != additions_.end()) {
    return iter->second;
  }

  NodePtr node = new Addition(lhs, rhs);
  additions_[key] = node;
  return node;
}

NodePtr NodeFactory::NewMultiplication(NodePtr lhs, NodePtr rhs) {
  // Optimizations
  if (lhs == empty_ || rhs == empty_) return empty_;
  if (lhs == epsilon_) return rhs;
  if (rhs == epsilon_) return lhs;

  auto key = std::make_pair(lhs, rhs);
  auto iter = multiplications_.find(key);
  if (iter != multiplications_.end()) {
    return iter->second;
  }

  NodePtr node = new Multiplication(lhs, rhs);
  multiplications_[key] = node;
  return node;
}

NodePtr NodeFactory::NewStar(NodePtr node) {
  if (node == empty_ || node == epsilon_) {
    return epsilon_;
  }

  auto iter = stars_.find(node);
  if (iter != stars_.end()) {
    return iter->second;
  }

  NodePtr star_node = new Star(node);
  stars_[node] = star_node;
  return star_node;
}

NodePtr NodeFactory::NewElement(VarId var) {
  auto iter = elems_.find(var);
  if (iter != elems_.end()) {
    return iter->second;
  }

  NodePtr node = new Element(var);
  elems_[var] = node;
  return node;
}

void NodeFactory::PrintDot(std::ostream &out) {
  out << "digraph FreeStructure {" << std::endl;
  
  std::set<NodePtr> visited;
  std::function<void(NodePtr)> traverse = [&](NodePtr node) {
    if (!node || visited.count(node)) return;
    visited.insert(node);
    
    if (auto add = dynamic_cast<const Addition*>(node)) {
      out << "  node" << node << " [label=\"+\"];" << std::endl;
      out << "  node" << node << " -> node" << add->GetLhs() << ";" << std::endl;
      out << "  node" << node << " -> node" << add->GetRhs() << ";" << std::endl;
      traverse(add->GetLhs());
      traverse(add->GetRhs());
    } else if (auto mul = dynamic_cast<const Multiplication*>(node)) {
      out << "  node" << node << " [label=\"*\"];" << std::endl;
      out << "  node" << node << " -> node" << mul->GetLhs() << ";" << std::endl;
      out << "  node" << node << " -> node" << mul->GetRhs() << ";" << std::endl;
      traverse(mul->GetLhs());
      traverse(mul->GetRhs());
    } else if (auto star = dynamic_cast<const Star*>(node)) {
      out << "  node" << node << " [label=\"*\"];" << std::endl;
      out << "  node" << node << " -> node" << star->GetNode() << ";" << std::endl;
      traverse(star->GetNode());
    } else if (auto elem = dynamic_cast<const Element*>(node)) {
      out << "  node" << node << " [label=\"" 
          << Var::GetVar(elem->GetVar()).string() << "\"];" << std::endl;
    } else if (dynamic_cast<const Epsilon*>(node)) {
      out << "  node" << node << " [label=\"1\"];" << std::endl;
    } else if (dynamic_cast<const Empty*>(node)) {
      out << "  node" << node << " [label=\"0\"];" << std::endl;
    }
  };
  
  // Traverse all roots
  for (const auto &pair : additions_) traverse(pair.second);
  for (const auto &pair : multiplications_) traverse(pair.second);
  for (const auto &pair : stars_) traverse(pair.second);
  
  out << "}" << std::endl;
}

void NodeFactory::GC() {
  // Simplified GC - just clear everything (for now)
  // A real implementation would use reference counting
}

void NodeFactory::PrintStats(std::ostream &out) {
  out << "FreeStructure Statistics:" << std::endl;
  out << "  Additions: " << additions_.size() << std::endl;
  out << "  Multiplications: " << multiplications_.size() << std::endl;
  out << "  Stars: " << stars_.size() << std::endl;
  out << "  Elements: " << elems_.size() << std::endl;
}

} // namespace fpsolve

