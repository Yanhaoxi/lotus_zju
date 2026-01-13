// Implementation of PriorityAssigner.
//
// Assigns topological priorities to CFG nodes.
//
// Purpose:
// The worklist algorithm (in the Engine) uses priorities to determine the order
// of node processing. Processing nodes in topological order (Reverse Post Order)
// significantly speeds up convergence for forward data flow analysis.
//
// Algorithm:
// Performs a simple DFS to compute post-order numbering, then assigns priorities
// based on that (essentially topological sort).

#include "Alias/TPA/PointerAnalysis/FrontEnd/CFG/PriorityAssigner.h"

#include "Alias/TPA/PointerAnalysis/Program/CFG/CFG.h"

namespace tpa {

void PriorityAssigner::traverse() {
  currLabel = 1;
  // Iterate all nodes to ensure we cover disconnected components (though reachable ones matter most).
  for (auto *node : cfg)
    if (node->getPriority() == 0u)
      visitNode(node);
}

void PriorityAssigner::visitNode(tpa::CFGNode *node) {
  if (!visitedNodes.insert(node).second)
    return;

  for (auto const &succ : node->succs())
    visitNode(succ);

  // Assign label in post-order.
  // Note: RPO would usually push to a stack and label on pop?
  // Here we just increment. Higher label = processed later?
  // If worklist is a priority queue popping highest/lowest?
  // Usually for forward analysis, we want RPO.
  node->setPriority(currLabel);
  ++currLabel;
}

} // namespace tpa