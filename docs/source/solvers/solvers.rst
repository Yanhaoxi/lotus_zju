Solver Frameworks
================

Constraint solving and decision procedures.

CUDD (Binary Decision Diagrams)
------------------------------

BDD-based symbolic manipulation and solving.

**Location**: ``lib/Solvers/CUDD/``

**Features**:
* **BDD operations** – Boolean function manipulation
* **Symbolic sets** – Set operations on BDDs
* **Fixed-point computation** – Iterative solving algorithms

**Usage**:
.. code-block:: cpp

   #include <Solvers/CUDD/CUDDManager.h>
   CUDDManager manager;
   auto bdd = manager.createBDD();

FPSolve (Fixed-Point Solvers)
----------------------------

General fixed-point computation algorithms.

**Location**: ``lib/Solvers/FPSolve/``

**Features**:
* **Fixed-point engines** – Chaotic iteration, worklist algorithms
* **Convergence acceleration** – Optimization techniques
* **Extensible framework** – Custom fixed-point problems

**Usage**:
.. code-block:: cpp

   #include <Solvers/FPSolve/FPSolver.h>
   FPSolver solver(problem);
   auto solution = solver.solve();

SMT (Satisfiability Modulo Theories)
-----------------------------------

SMT solving infrastructure and theory integrations.

**Location**: ``lib/Solvers/SMT/``

**Components**:
* **Z3 integration** – Z3 SMT solver backend
* **Theory solvers** – Arithmetic, arrays, bitvectors
* **Formula construction** – SMT formula building utilities

**Usage**:
.. code-block:: cpp

   #include <Solvers/SMT/SMTContext.h>
   SMTContext ctx;
   auto solver = ctx.createSolver();
   solver->add(formula);

WPDS (Weighted Pushdown Systems)
-------------------------------

Weighted pushdown system solver.

**Location**: ``lib/Solvers/WPDS/``

**Features**:
* **Weighted automata** – Weighted transition systems
* **Pushdown model** – Stack-based computation model
* **Path analysis** – Weighted path computation

**Usage**:
.. code-block:: cpp

   #include <Solvers/WPDS/WPDSSystem.h>
   WPDSSystem system;
   auto solution = system.solve();
