Data Flow Analysis Frameworks
============================

Advanced data flow analysis algorithms and frameworks.

IFDS (Interprocedural Finite Distributive Subset)
------------------------------------------------

Interprocedural finite distributive subset analysis framework.

**Location**: ``lib/Dataflow/IFDS/``

**Features**:
* **Interprocedural analysis** – Cross-function data flow
* **Distributive framework** – Meet-over-all-paths semantics
* **Tabulation algorithm** – Efficient fixpoint computation

**Usage**:
.. code-block:: cpp

   #include <Dataflow/IFDS/IFDSAnalysis.h>
   IFDSAnalysis ifds(problem);
   auto results = ifds.solve();

Mono (Monotone Frameworks)
-------------------------

Monotone data flow analysis framework.

**Location**: ``lib/Dataflow/Mono/``

**Features**:
* **Monotone functions** – Guaranteed convergence properties
* **Worklist algorithms** – Efficient iterative solving
* **Extensible design** – Custom analysis implementations

**Usage**:
.. code-block:: cpp

   #include <Dataflow/Mono/MonotoneSolver.h>
   MonotoneSolver solver(problem);
   auto solution = solver.solve();

NPA (Non-deterministic Pushdown Automata)
----------------------------------------

Pushdown automata for modeling non-deterministic behavior.

**Location**: ``lib/Dataflow/NPA/``

**Features**: Modeling of non-deterministic program behaviors using pushdown automata.

WPDS (Weighted Pushdown Systems)
-------------------------------

Weighted pushdown system analysis framework.

**Location**: ``lib/Dataflow/WPDS/``

**Features**:
* **Weighted semantics** – Cost or distance annotations
* **Pushdown systems** – Stack-based analysis
* **Interprocedural analysis** – Function call handling

**Usage**:
.. code-block:: cpp

   #include <Dataflow/WPDS/WPDSSolver.h>
   WPDSSolver solver(system);
   auto weightedPaths = solver.analyze();
