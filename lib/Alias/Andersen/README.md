# Andersen Analysis

from https://github.com/grievejia/andersen

## Optimizations

The implementation includes four optional optimizations that can be enabled via command-line flags:

| Optimization | Paper | Option |
|--------------|-------|--------|
| **HVN** | "Exploiting Pointer and Location Equivalence to Optimize Pointer Analysis" (SAS 2007) | `--enable-hvn` |
| **HU** | "Exploiting Pointer and Location Equivalence to Optimize Pointer Analysis" (SAS 2007) | `--enable-hu` |
| **HCD** | "The Ant and the Grasshopper: Fast and Accurate Pointer Analysis for Millions of Lines of Code" (PLDI 2007) | `--enable-hcd` |
| **LCD** | "The Ant and the Grasshopper: Fast and Accurate Pointer Analysis for Millions of Lines of Code" (PLDI 2007) | `--enable-lcd` |

**Descriptions:**
- **HVN** (Hash-based Value Numbering): Performs offline variable substitution without evaluating unions to identify pointer equivalences and merge nodes
- **HU** (Heintze-Ullman): Performs offline variable substitution including union evaluation to identify pointer and location equivalences
- **HCD** (Hybrid Cycle Detection): Offline cycle detection that identifies collapse targets before constraint solving, then applies them during online solving
- **LCD** (Lazy Cycle Detection): Online cycle detection that batches cycle candidates and checks them together for efficiency

All optimizations are **disabled by default**. HVN and HU run during the constraint optimization phase, while HCD and LCD run during the constraint solving phase.