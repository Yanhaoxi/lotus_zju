Optimization Passes
===================

Lotus provides several optimization passes located in ``lib/Optimization/``.
These passes are useful for preparing code for analysis or for improving
performance.

**Headers**: ``include/Optimization``

**Implementation**: ``lib/Optimization``

ModuleOptimizer
---------------

**File**: ``ModuleOptimizer.cpp``, ``ModuleOptimizer.h``

Provides a simple interface for applying standard LLVM optimization pipelines
(O0, O1, O2, or O3) to an entire module.

**Usage**:

.. code-block:: cpp

   #include <Optimization/ModuleOptimizer.h>

   llvm::Module *M = ...;
   
   // Apply O2 optimizations
   llvm_utils::optimiseModule(M, llvm::OptimizationLevel::O2);

**When to use**:

- Apply standard LLVM optimizations before running analyses
- Prepare code for verification or further transformation
- Test analysis tools on optimized code

AInliner (Aggressive Inliner)
------------------------------

**File**: ``AInliner.cpp``

An aggressive inliner that attempts to inline as many function calls as
possible. This pass is tuned for analysis-friendly IR where maximum inlining
may be beneficial.

**Features**:

- Inlines function calls aggressively
- Supports exclusion list via ``-ainline-noinline`` command-line option
- Operates at the module level

**Command-line options**:

.. code-block:: bash

   -ainline-noinline="func1,func2"  # Functions to exclude from inlining

**When to use**:

- Reduce inter-procedural complexity for analysis
- Improve precision of intra-procedural analyses
- Prepare code where maximum inlining is desired

**Limitations**:

- May significantly increase code size
- Use with caution on large codebases

LICM (Loop Invariant Code Motion)
----------------------------------

**File**: ``LICM.cpp``, ``LICM.h``

Loop Invariant Code Motion pass taken from LLVM 14. This pass moves loop-invariant
code out of loops to improve performance.

**Features**:

- Hoists loop-invariant instructions to loop preheader
- Sinks code to exit blocks when safe
- Promotes must-aliased memory locations to registers
- Uses alias analysis for precise memory operations

**When to use**:

- Optimize loops by moving invariant computations outside
- Evaluate alias analysis precision (LICM relies on alias analysis)
- Improve code before analysis or verification

**Note**: This is the LLVM 14 version of LICM, used internally by Lotus for
evaluating alias analysis algorithms.

SWPrefetching (Software Prefetching)
-------------------------------------

**File**: ``SWPrefetching.cpp``, ``SWPrefetching.h``

Implements profile-guided software prefetching for indirect memory accesses.
This pass injects ``llvm.prefetch`` intrinsics to reduce cache misses.

**Features**:

- Profile-guided prefetch distance computation
- Supports Sample FDO profiles or user-provided LBR (Last Branch Record) distances
- Discovers loop-carried induction variables that feed load addresses
- Clones dependence chains to compute future addresses before issuing prefetches

**Command-line options**:

.. code-block:: bash

   -input-file=<filename>        # Input file for profiles
   -dist=<value>                  # Specify offset values from LBR (one or more)

**Algorithm**:

The pass:

1. Loads sample FDO profiles or uses provided LBR distances
2. Discovers loop-carried induction variables feeding load addresses
3. Clones the dependence chain to compute a future address
4. Issues ``llvm.prefetch`` intrinsics at the computed distance

**When to use**:

- Improve cache performance for memory-bound loops
- Reduce cache misses for indirect memory accesses
- Optimize hot paths identified by profiling

**Related work**:

- CGO 17: Software Prefetching for Indirect Memory Accesses (Sam Ainsworth, Timothy M. Jones)
- TOCS 19: Software Prefetching for Indirect Memory Accesses: A Microarchitectural Perspective
- EuroSys'22: PT-GET: profile-guided timely software prefetching

**Benchmarks**: https://github.com/masabahmad/CRONO

Integration with Analysis
-------------------------

These optimization passes can be used to prepare code for analysis:

.. code-block:: cpp

   #include <Optimization/ModuleOptimizer.h>
   #include <Optimization/SWPrefetching.h>
   
   llvm::Module &M = ...;
   
   // Apply standard optimizations
   llvm_utils::optimiseModule(&M, llvm::OptimizationLevel::O1);
   
   // Then run analyses
   // ...

**Typical pipeline**:

1. Apply optimizations (ModuleOptimizer, AInliner, etc.)
2. Run alias analysis or other static analyses
3. Perform verification or bug detection
