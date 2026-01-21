# TPA: Flow- and Context-Sensitive Pointer Analysis

TPA is an **inclusion-based**, **flow- and context-sensitive** pointer analysis framework with k-limiting support. It uses a **semi-sparse** program representation to achieve both precision and scalability for large C/C++ programs.

## Key Features

- **Flow-sensitivity**: Respects program order and control flow
- **Context-sensitivity**: Distinguishes different calling contexts (with k-limiting support)
- **Semi-sparse representation**: Only analyzes relevant program points (def-use chains)
- **Field-sensitive memory model**: Tracks memory objects at the field/element level

## Architecture

```
LLVM IR → IR Transforms → Front-End Processing → Global Initialization → Semi-Sparse Analysis → Points-to Sets
```

## Directory Structure

- **`PointerAnalysis/`**: Core analysis implementation
  - `Analysis/`: Main analysis classes (`SemiSparsePointerAnalysis`, `GlobalPointerAnalysis`)
  - `Engine/`: Worklist propagation, transfer functions, store pruning
  - `FrontEnd/`: LLVM IR to internal representation conversion
  - `MemoryModel/`: Memory object and pointer management
  - `Context/`: Context sensitivity (`Context`, `KLimitContext`, `AdaptiveContext`)
  - `Program/`: Semi-sparse program representation
  - `Support/`: Data structures (`Env`, `Store`, `PtsSet`)

- **`Transforms/`**: LLVM IR normalization passes (GEP expansion, constant folding, etc.)

- **`Util/`**: Utilities (IO, data structures, iterators)

## Usage

### Command-Line Tool

```bash
tpa [options] <input bitcode file>
```

**Key options:**
- `-k-limit <n>`: Set k-limit for context-sensitive analysis (0 = context-insensitive)
- `-ext <file>`: External pointer table file
- `-print-pts`: Print points-to sets
- `-cfg-dot-dir <dir>`: Output CFG dot files

### Programmatic Usage

```cpp
SemiSparseProgramBuilder builder;
auto ssProg = builder.build(module);

SemiSparsePointerAnalysis analysis;
analysis.runOnProgram(ssProg);

auto ptsSet = analysis.getPtsSet(pointer);
```

## Analysis Characteristics

| Characteristic | Value |
|----------------|-------|
| **Analysis Type** | Inclusion-based (Andersen-style) |
| **Flow-Sensitive** | ✅ Yes |
| **Context-Sensitive** | ✅ Yes (with k-limiting) |
| **Field-Sensitive** | ✅ Yes (type-based) |
| **Representation** | Semi-sparse |

## See Also

- Parent README: `lib/Alias/README.md`
- Documentation: `docs/source/alias/tpa.rst`
