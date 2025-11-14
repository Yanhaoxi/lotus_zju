Analysis Components
==================

Lotus provides various static analysis utilities and frameworks for program analysis tasks.

CFG Analysis
------------

Control Flow Graph analysis utilities for reachability and structural analysis.

**Location**: ``lib/Analysis/CFG``

**Components**:
* **CFGReachability** – Control flow reachability analysis
* **CodeMetrics** – Code complexity and size metrics
* **Dominator** – Dominator tree analysis
* **TopologicalOrder** – CFG topological ordering algorithms

**Usage**:
.. code-block:: cpp

   #include <Analysis/CFG/CFGReachability.h>
   CFGReachability reach(module);
   bool reachable = reach.isReachable(fromBB, toBB);

Concurrency Analysis
-------------------

Thread-aware analysis for concurrent programs.

**Location**: ``lib/Analysis/Concurrency``

**Components**:
* **LockSetAnalysis** – Lock set analysis for race detection
* **MemUseDefAnalysis** – Memory use-definition analysis in concurrent contexts
* **MHPAnalysis** – May-Happen-in-Parallel analysis
* **ThreadAPI** – Thread creation and synchronization APIs

**Usage**:
.. code-block:: cpp

   #include <Analysis/Concurrency/MHPAnalysis.h>
   MHPAnalysis mhp(function);
   bool mayParallel = mhp.mayHappenInParallel(inst1, inst2);

GVFA (Global Value Flow Analysis)
---------------------------------

Global value flow analysis for tracking value propagation across functions.

**Location**: ``lib/Analysis/GVFA``

**Components**:
* **GlobalValueFlowAnalysis** – Interprocedural value flow tracking
* **GVFAUtils** – Utility functions for GVFA
* **ReachabilityAlgorithms** – Value reachability algorithms

**Usage**:
.. code-block:: cpp

   #include <Analysis/GVFA/GlobalValueFlowAnalysis.h>
   GlobalValueFlowAnalysis gvfa(module);
   auto flow = gvfa.analyzeValue(value);

Null Pointer Analysis
--------------------

Multiple null pointer detection and analysis algorithms.

**Location**: ``lib/Analysis/NullPointer``

**Components**:
* **AliasAnalysisAdapter** – AA integration for null checking
* **ContextSensitiveLocalNullCheckAnalysis** – Local context-sensitive analysis
* **ContextSensitiveNullCheckAnalysis** – Interprocedural context-sensitive analysis
* **ContextSensitiveNullFlowAnalysis** – Null value flow tracking
* **LocalNullCheckAnalysis** – Intraprocedural null checking
* **NullCheckAnalysis** – General null pointer analysis
* **NullEquivalenceAnalysis** – Null value equivalence classes
* **NullFlowAnalysis** – Null propagation analysis

**Usage**:
.. code-block:: cpp

   #include <Analysis/NullPointer/NullCheckAnalysis.h>
   NullCheckAnalysis npa(function);
   bool mayBeNull = npa.mayBeNull(pointer);
