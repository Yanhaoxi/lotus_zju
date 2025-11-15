Application Frameworks
====================

Lotus includes several application frameworks for specific analysis tasks.

Checker Framework
-----------------

General-purpose bug checking utilities and analyzers.

**Location**: ``lib/Apps/Checker/``

**Components**:
* **concurrency/** – Race condition and deadlock detection
* **gvfa/** – Global value flow bug checking
* **kint/** – Numerical analysis checkers
* **Report/** – Bug report generation and formatting

**Usage**: Integrated into various analysis pipelines for automated bug detection.

CLAM (CLang Abstract Machine)
-----------------------------

Abstract interpretation framework for numerical analysis.

**Location**: ``lib/Apps/clam/``

**Features**:
* **Abstract Domains** – Intervals, congruences, polyhedra, arrays
* **Analysis Engines** – Forward/backward analysis, invariant generation
* **Transformations** – Abstract domain operations and optimizations

**Build Target**: ``tools/clam``

**Usage**:
.. code-block:: bash

   ./build/bin/clam --crab-domains=intervals example.bc

**Key Options**:
* ``--crab-domains=<domain>``: Analysis domain (intervals, octagons, polyhedra)
* ``--crab-inter``: Interprocedural analysis
* ``--crab-widening-delay=<n>``: Widening delay parameter

Fuzzing Support
--------------

Fuzzing utilities and instrumentation for test generation.

**Location**: ``lib/Apps/Fuzzing/``

**Components**:
* **Fuzzing instrumentation passes**
* **Coverage tracking utilities**
* **Input generation helpers**

**Build Target**: ``tools/fuzzing``

**Usage**: Instrumentation for coverage-guided fuzzing workflows.

MCP (Model Checking Platform)
-----------------------------

Model checking framework integration.

**Location**: ``lib/Apps/MCP/``

**Features**: Model checking algorithm implementations and verification utilities.

**Build Target**: ``tools/mcp``

SeaHorn Verification Framework
------------------------------

Large-scale verification framework with SMT-based model checking.

**Location**: ``lib/Apps/seahorn/``

**Features**:
* **Horn clause solving** – CHC-based verification
* **Abstract interpretation** – Numerical and shape analysis
* **Counterexample generation** – Error trace production
* **Modular verification** – Compositional reasoning

**Build Target**: ``tools/seahorn``

For a detailed usage and workflow description, see :doc:`seahorn`.
