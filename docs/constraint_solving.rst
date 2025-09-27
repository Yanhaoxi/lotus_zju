Constraint Solving
===================

Lotus provides several constraint solving backends for program analysis.

SMT Solving
-----------

SMT solving using Z3 4.11 integration.

**Location**: ``lib/Solvers/SMT``

**Features**: Bit-vectors, arrays, uninterpreted functions, arithmetic

**Usage**:
.. code-block:: cpp

   #include <Solvers/SMT/SATSolver.h>
   auto solver = std::make_unique<SATSolver>();
   solver->addConstraint(expr1);
   auto result = solver->check();

**Tool**: ``./build/bin/owl formula.smt2``

Binary Decision Diagrams (BDD)
------------------------------

BDD implementation using CUDD library.

**Location**: ``lib/Solvers/CUDD``

**Features**: Boolean operations, quantification, set operations

**Usage**:
.. code-block:: cpp

   #include <Solvers/CUDD/BDD.h>
   auto bddManager = std::make_unique<BDDManager>();
   auto var1 = bddManager->createVariable("x");
   auto bdd = var1 & var2;  // x AND y

Weighted Pushdown System (WPDS)
-------------------------------

WPDS library for interprocedural program analysis.

**Location**: ``lib/Solvers/WPDS``

**Features**: Pushdown systems, weight domains, reachability analysis

**Usage**:
.. code-block:: cpp

   #include <Solvers/WPDS/WPDS.h>
   auto wpds = std::make_unique<WPDS>();
   wpds->addRule(fromState, stackSymbol, toState, weight);
   auto result = wpds->reachability(initialState, targetState);

Solver Selection
----------------

| Solver | Best For | Performance | Memory |
|--------|----------|-------------|---------|
| SMT (Z3) | Complex constraints, bit-vectors | Good | Moderate |
| BDD (CUDD) | Boolean constraints, sets | Excellent | Can be high |
| WPDS | Interprocedural analysis | Good | Moderate |

**Unified Interface**:
.. code-block:: cpp

   auto solver = createSolver(SolverType::SMT);
   auto solver = createSolver(SolverType::BDD);
   auto solver = createSolver(SolverType::WPDS);
