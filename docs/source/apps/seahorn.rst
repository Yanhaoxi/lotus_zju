SeaHorn Verification Framework
==============================

Large-scale SMT-based verification framework built on constrained Horn clauses
(CHC), symbolic execution, and abstraction-refinement.

Overview
--------

SeaHorn performs bounded and unbounded model checking on LLVM bitcode using SMT
solvers to prove program correctness or produce counterexamples. It integrates
with Lotus analyses and solver backends to support complex verification
pipelines.

**Location**: ``lib/Apps/seahorn/``

**Build Targets**:

- ``tools/seahorn``
- ``tools/verifier/seahorn``
- ``tools/verifier/horn-ice``

Components
----------

- **SeaHorn** – LLVM-based front-end, symbolic execution, and encoding of
  verification problems as CHCs.
- **Horn-ICE** – CHC solving with invariant learning.
- **Sea-rt** – Runtime components for executing counterexample harnesses.

SeaHorn Verification
--------------------

Main verification tool for C programs.

**Basic usage**:

.. code-block:: bash

   ./build/bin/seahorn [options] <input.c>

**Common modes**:

- ``--bmc=<N>`` – Bounded model checking up to ``N`` steps.
- ``--horn`` – CHC-based (unbounded) verification.
- ``--abstractor=clam`` – Use CLAM-based abstract interpretation as an
  abstractor.

**Frequently used options**:

- ``--cex=<file>`` – Dump a counterexample harness to ``<file>``.
- ``--track=mem`` – Track memory (heap/stack) explicitly.
- ``--horn-solver=spacer|ice`` – Select CHC solving engine.

Counterexample Analysis
~~~~~~~~~~~~~~~~~~~~~~~

Generate executable counterexamples:

.. code-block:: bash

   ./build/bin/seahorn --cex=harness.ll program.c
   clang -m64 -g program.c harness.ll -o counterexample

Horn-ICE CHC Verification
-------------------------

CHC verification with invariant learning.

**Usage**:

.. code-block:: bash

   ./build/bin/chc-verifier <input.smt2>
   ./build/bin/hice-dt <input.smt2>  # With learning

**CHC Format**: SMT-LIB2 with Horn clauses

Verification Workflow
---------------------

1. Compile: ``clang -c -emit-llvm program.c -o program.bc``
2. Verify: ``./build/bin/seahorn program.c``
3. Debug: ``./build/bin/seahorn --cex=harness.ll program.c``

Solver Guide
------------

SeaHorn relies on several solver backends described in :doc:`../solvers/index`:

.. list-table::
   :header-rows: 1
   :widths: 18 32 32

   * - Solver
     - Strengths
     - Best For
   * - Spacer
     - Fast, robust CHC solving in Z3
     - General safety verification and reachability
   * - Horn-ICE
     - Invariant learning via ICE-style algorithms
     - Programs requiring complex invariants
   * - CLAM
     - Abstract interpretation over numerical domains
     - Numerical properties and over-approximate invariants


