Transform Utilities
==================

LLVM bitcode transformation and optimization passes.

Core Transforms
---------------

Essential transformation passes for analysis preparation.

**Location**: ``lib/Transform/``

**ElimPhi** – PHI node elimination
  Converts PHI nodes to select instructions for simpler analysis.

**ExpandAssume** – Assume intrinsic expansion
  Expands LLVM assume intrinsics into explicit branches.

**LowerConstantExpr** – Constant expression lowering
  Converts constant expressions to instructions.

**LowerSelect** – Select instruction lowering
  Converts select instructions to branches.

**MergeReturn** – Return instruction merging
  Unifies multiple return instructions into a single exit.

**Usage**:
.. code-block:: cpp

   #include <Transform/LowerConstantExpr.h>
   LowerConstantExpr pass;
   bool changed = pass.runOnModule(module);

Memory and Data Transforms
--------------------------

Memory layout and data structure transformations.

**LowerGlobalConstantArraySelect** – Global array select lowering
**MergeGEP** – GEP instruction merging
**SimplifyExtractValue** – ExtractValue simplification
**SimplifyInsertValue** – InsertValue simplification

Control Flow Transforms
----------------------

Control flow structure modifications.

**RemoveDeadBlock** – Dead block elimination
**RemoveNoRetFunction** – Non-returning function removal
**SimplifyLatch** – Loop latch simplification
**NameBlock** – Basic block naming

Optimization Transforms
----------------------

Advanced optimization and cleanup passes.

**ModuleOptimizer** – Module-level optimizations
**SoftFloat** – Software floating-point implementation
**UnrollVectors** – Vector instruction unrolling
**Unrolling** – Loop unrolling transformations

**AInliner** – Aggressive inlining pass

**Usage Examples**:
.. code-block:: cpp

   // Multiple transforms can be chained
   ModuleOptimizer optimizer;
   UnrollVectors unroller;
   bool changed = optimizer.runOnModule(module) ||
                  unroller.runOnModule(module);
