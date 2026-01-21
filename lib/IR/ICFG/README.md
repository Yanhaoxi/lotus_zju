# ICFG: Interprocedural Control Flow Graph

The **Interprocedural Control Flow Graph (ICFG)** is the primary control-flow representation used by many analyses in Lotus. It extends the standard intraprocedural CFG with **call** and **return** edges to capture interprocedural control flow.

The ICFG serves as a backbone graph on top of which many of the dataflow engines in Lotus operate. It provides a unified view of control flow across function boundaries, enabling whole-program analysis.

## Key Features

- **Interprocedural Edges**: Call and return edges connecting caller and callee code
- **Node Types**: Support for intraprocedural blocks, function entry points, and return points
- **Graph Analysis Utilities**: Back edge detection, reachability queries, and shortest path computation
- **Cycle Removal**: Optional removal of intraprocedural and interprocedural cycles for acyclic analysis
- **Call Graph Integration**: Built-in support for call graph construction and traversal
- **Context-Aware Traversals**: Support for context-sensitive traversals used by dataflow and reachability engines

## Components

- **`ICFG.cpp`**: Main graph class extending `GenericGraph<ICFGNode, ICFGEdge>`, provides node/edge management and traversal interfaces
- **`ICFGBuilder.cpp`**: Constructs ICFG from LLVM module, processes functions to create intraprocedural and interprocedural edges
- **`CallGraph.cpp`**: Custom call graph implementation with explicit call relationships
- **`GraphAnalysis.cpp`**: Utility functions for back edge detection, reachability, shortest paths

## Usage

```cpp
#include "IR/ICFG/ICFG.h"
#include "IR/ICFG/ICFGBuilder.h"

// Create an empty ICFG
ICFG* icfg = new ICFG();

// Build ICFG from LLVM module
ICFGBuilder builder(icfg);
builder.setRemoveCycleAfterBuild(false);
builder.build(&module);

// Iterate over nodes
for (auto *node : icfg->getNodes()) {
  // Process node
}

// Query edges
for (auto *edge : icfg->getEdges()) {
  if (edge->isCallCFGEdge()) {
    // Handle call edge
  }
}
```

## Integration

The ICFG is used as the foundation for many analyses in Lotus:

- **IFDS/IDE**: Dataflow framework uses ICFG to traverse interprocedural paths
- **WPDS**: Weighted Pushdown Systems use ICFG to model interprocedural control flow
- **PDG**: Program Dependence Graph is built on top of the ICFG
- **Reachability Analyses**: Various path-sensitive analyses use the ICFG
- **CFL-Reachability**: Context-free language reachability analyses operate over the ICFG

## See Also

- Headers: `include/IR/ICFG/`
- Documentation: `docs/source/ir/icfg.rst`
