===========================
AserPTA — Pointer Analysis
===========================

Overview
========

AserPTA is a **configurable pointer analysis framework** providing multiple
context sensitivities, memory models, and solver algorithms. It targets
large C/C++ programs and supports both **whole-program** and **modular**
analyses.

* **Location**: ``lib/Alias/AserPTA``
* **Design**: Constraint-based, graph-driven
* **Key Features**:
  - Multiple context sensitivities (CI, K-call-site, K-origin)
  - Field-insensitive and field-sensitive memory models
  - Several solver backends optimized for scalability

Architecture
============

The analysis pipeline is organized as:

1. **IR Preprocessing** (``PreProcessing/``)
   - Canonicalizes GEPs, lowers ``memcpy``, normalizes heap APIs.
   - Removes exception handlers and inserts synthetic initializers.
2. **Constraint Collection** (``PointerAnalysis/``)
   - Extracts five core constraint types:
     ``addr_of``, ``copy``, ``load``, ``store``, ``offset``.
3. **Constraint Graph Construction**
   - Nodes: pointer nodes (CGPtrNode), object nodes (CGObjNode),
     SCC nodes (CGSuperNode).
4. **Solving with Context**
   - Context models: ``NoCtx``, ``KCallSite<K>``, ``KOrigin<K>``.
   - Solver choices: Andersen, WavePropagation, DeepPropagation,
     PartialUpdateSolver.

Analysis Modes and Options
===========================

Common configuration flags:

* ``-analysis-mode=<mode>`` – ``ci``, ``1-cfa``, ``2-cfa``, ``origin``.
* ``-solver=<type>`` – ``basic``, ``wave``, ``deep``.
* ``-field-sensitive`` / ``-field-insensitive`` – choose memory model.
* ``-dump-stats`` – print statistics.
* ``-consgraph`` – dump constraint graph in DOT format.

Usage
=====

The standalone driver can be invoked as:

.. code-block:: bash

   ./build/bin/aser-aa -analysis-mode=1-cfa -solver=deep example.bc

In Lotus, AserPTA is also accessible through the AA wrapper and configuration
files that select it as the primary alias analysis engine.


