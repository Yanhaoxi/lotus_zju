# SSI: Static Single Information

**Static Single Information (SSI)** is an extension of SSA (Static Single Assignment) that augments variables with additional predicate and path information. While SSA ensures that every definition of a variable dominates all its uses, SSI additionally guarantees that **every use of a variable post-dominates all its reaching definitions**.

The SSI transformation converts SSA-form IR into SSI form by introducing **sigma (σ) functions** at control-flow splits, analogous to how SSA introduces phi (φ) functions at control-flow joins.

## Key Features

- **Dual Dominance Property**: Extends SSA's dominance property with post-dominance
- **Sigma Functions**: Introduces σ-functions at control-flow splits to encode predicate information
- **Path-Sensitive Analysis**: Provides explicit representation of values along different control-flow paths
- **Condition-Sensitive Reasoning**: Encodes relationships between values guarded by conditions
- **Live-Range Splitting**: Splits variable live ranges at strategic program points

## SSI Construction

SSI form is constructed as follows:

1. **Start from SSA form**: Assumes input IR is already in SSA form (with φ-functions at joins)
2. **Compute iterated post-dominance frontier**: Determines where σ-functions are needed
3. **Insert σ-functions**: At each control-flow split whose successors are not in the same post-domination tree region
4. **Rename variables**: Assign unique names to σ results, mirroring the SSA renaming process

## Components

- **`SSIPass.cpp`**: Main transformation pass entry point
- **`SSITransform.cpp`**: Core transformation logic that converts SSA to SSI form
  - `run()`: Determines splitting strategy and invokes split/rename operations
  - `split()`: Splits live ranges at strategic program points
  - `rename()`: Renames variables to maintain SSI invariants
  - `clean()`: Removes unnecessary SSI nodes
- **`SSIUtils.cpp`**: Utility functions for program points, renaming stacks, post-dominance frontiers

## Usage

```cpp
#include "IR/SSI/SSI.h"

// SSI transformation is typically run as part of a pass pipeline
// The pass requires DominatorTree, PostDominatorTree, and DominanceFrontier
FunctionPass *createSSIfyPass();
```

## Command-Line Options

- `-v`: Enable verbose mode, printing detailed transformation information
- `-set xxxx`: Configure initial program points (each x is 0 or 1):
  - **1st bit**: Exit of conditionals, downwards
  - **2nd bit**: Exit of conditionals, upwards
  - **3rd bit**: Uses, downwards
  - **4th bit**: Uses, upwards

Example: `-set 1100` enables splitting at conditional exits (both directions) but not at uses.

## Checking for SSI Instructions

```cpp
#include "IR/SSI/SSI.h"

Instruction *I = ...;
if (isSigma(I)) {
  // Handle σ-function
} else if (isPhi(I)) {
  // Handle φ-function
}
```

## Integration

SSI is used by:
- **SRAA**: Symbolic Range Analysis uses vSSA (variant of SSI)
- **Path-Sensitive Analyses**: Analyses that need to reason about value relationships across conditional branches
- **Verification Tools**: Tools that require explicit path information

## See Also

- Headers: `include/IR/SSI/`
- Documentation: `docs/source/ir/ssi.rst`
