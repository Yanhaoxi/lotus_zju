Intermediate Representations
============================

Advanced IR constructions for program analysis.

ICFG (Interprocedural Control Flow Graph)
-----------------------------------------

Interprocedural control flow representation.

**Location**: ``lib/IR/ICFG/``

**Features**:
* **Interprocedural edges** – Function call and return connections
* **Context sensitivity** – Context-aware control flow
* **Graph algorithms** – Path analysis and reachability

**Usage**:
.. code-block:: cpp

   #include <IR/ICFG/ICFGBuilder.h>
   ICFGBuilder builder(module);
   auto icfg = builder.build();

PDG (Program Dependence Graph)
-----------------------------

Fine-grained data and control dependence representation.

**Location**: ``lib/IR/PDG/``

**Components**:
* **DataDependenceGraph** – Data dependence edges
* **ControlDependenceGraph** – Control dependence edges
* **ProgramDependenceGraph** – Combined data/control dependencies

**Features**:
* **Precise dependencies** – Field-sensitive and context-aware
* **Slicing algorithms** – Program slicing utilities
* **Impact analysis** – Change impact computation

**Usage**:
.. code-block:: cpp

   #include <IR/PDG/ProgramDependenceGraph.h>
   ProgramDependenceGraph pdg(function);
   auto slice = pdg.computeSlice(instruction);

SSI (Static Single Information)
------------------------------

Static single information form for SSA extensions.

**Location**: ``lib/IR/SSI/``

**Features**:
* **Extended SSA** – Beyond standard SSA form
* **Predicate information** – Conditional value information
* **Analysis foundation** – Basis for advanced optimizations

**Usage**:
.. code-block:: cpp

   #include <IR/SSI/SSIBuilder.h>
   SSIBuilder builder(function);
   auto ssi = builder.build();