# GSA: Gated SSA

GSA (Gated SSA) is a control-flow transformation that augments SSA by introducing **gating (gamma) functions** that explicitly encode the control flow guarding each value flowing into a join point.

The implementation is adapted from Havlak's construction of Thinned Gated Single-Assignment form (LCPC'93). GSA is intentionally independent from verification-specific utilities so it can be reused by general IR analyses and transformations.

## Key Features

- **Control Dependence Analysis**: Computes control dependence relationships between basic blocks
- **Gamma Nodes**: Materializes gating functions (gamma nodes) for PHI nodes that explicitly encode control flow conditions
- **Thinned GSA**: Optional thinned version that reduces the use of undef values
- **PHI Replacement**: Optionally replaces PHI nodes with computed gamma nodes in the IR

## Components

- **`ControlDependenceAnalysis.cpp`**: Computes which basic blocks a given block is control dependent on, provides reachability queries, maintains topological ordering
- **`GateAnalysis.cpp`**: Builds the Gated SSA representation by materializing gamma nodes for PHI nodes, supports both standard and thinned GSA forms

## Usage

```cpp
#include "IR/GSA/GSA.h"

// Create control dependence analysis pass
auto *CDA = createControlDependenceAnalysisPass();

// Create gate analysis pass
auto *GA = createGateAnalysisPass();

// Run passes on module
CDA->runOnModule(M);
GA->runOnModule(M);

// Query gate analysis for a function
gsa::GateAnalysis &gateAnalysis = GA->getGateAnalysis(F);

// Get gamma value for a PHI node
PHINode *phi = ...;
Value *gamma = gateAnalysis.getGamma(phi);
```

## Command-Line Options

- `-gsa-thinned`: Emit thin gamma nodes (TGSA), default: true
- `-gsa-replace-phis`: Replace PHI nodes with gamma nodes in the IR, default: true

## Integration

GSA is used by:
- **SeaHorn**: For verification and symbolic execution
- **Control Flow Analyses**: For understanding control dependencies
- **IR Transformations**: For control-flow aware optimizations

## See Also

- Headers: `include/IR/GSA/`
- Documentation: `docs/source/ir/gsa.rst`
