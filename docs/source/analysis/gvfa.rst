GVFA (Global Value Flow Analysis)
=================================

Global value flow analysis for tracking value propagation across functions.

**Headers**: ``include/Analysis/GVFA``

**Implementation**: ``lib/Analysis/GVFA``

**Main components**:

- **GlobalValueFlowAnalysis** – Interprocedural value-flow engine
- **GVFAUtils** – Utility helpers for building and querying GVFA
- **ReachabilityAlgorithms** – Algorithms for value reachability queries

**Typical use cases**:

- Trace how a value propagates across function boundaries
- Answer “where can this value flow to?” queries
- Build higher-level dataflow or taint analyses on top of value-flow results

**Basic usage (C\+\+)**:

.. code-block:: cpp

   #include <Analysis/GVFA/GlobalValueFlowAnalysis.h>
   #include <Alias/DyckAA/DyckVFG.h>
   #include <Alias/DyckAA/DyckAliasAnalysis.h>
   #include <Alias/DyckAA/DyckModRefAnalysis.h>

   llvm::Module &M = ...;
   DyckVFG *VFG = /* constructed elsewhere */ nullptr;
   DyckAliasAnalysis *AA = /* constructed elsewhere */ nullptr;
   DyckModRefAnalysis *MRA = /* constructed elsewhere */ nullptr;

   DyckGlobalValueFlowAnalysis gvfa(&M, VFG, AA, MRA);
   gvfa.run();

   const llvm::Value *Src = ...;
   const llvm::Value *Sink = ...;
   bool flows = gvfa.cflReachable(Src, Sink);
   // Optionally, use GVFAUtils::getWitnessPath(...) to extract a witness path


