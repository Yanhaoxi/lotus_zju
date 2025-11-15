Data Flow Analysis
===================

This page documents **command-line tools** built on top of Lotus'
dataflow engines.
For the engines themselves, see :doc:`mono`, :doc:`ifds_ide`,
and :doc:`wpds`.

IFDS/IDE Framework
------------------

Lotus provides IFDS/IDE solvers in ``lib/Dataflow/IFDS``.
They are used by several analyses and tools described below;
see :doc:`ifds_ide` for algorithmic and API details.

Taint Analysis
--------------

Interprocedural taint analysis using IFDS framework.

**Location**: ``tools/checker/lotus_taint.cpp``

**Usage**:
.. code-block:: bash

   ./build/bin/lotus-taint [options] <input.bc>
   ./build/bin/lotus-taint -sources="read,scanf" -sinks="system,exec" input.bc

**Options**: ``-analysis=<N>``, ``-sources=<funcs>``, ``-sinks=<funcs>``, ``-verbose``

Global Value Flow Analysis (GVFA)
----------------------------------

Value flow analysis for memory safety bugs.

**Location**: ``tools/checker/lotus_gvfa.cpp``

**Usage**:
.. code-block:: bash

   ./build/bin/lotus-gvfa [options] <input.bc>
   ./build/bin/lotus-gvfa -vuln-type=nullpointer input.bc

**Types**: nullpointer (default), taint
**Options**: ``-vuln-type=<type>``, ``-dump-stats``, ``-verbose``

KINT Bug Finder
---------------

Integer and array bounds bug detection.

**Location**: ``tools/checker/lotus_kint.cpp``

**Checks**: overflow, division by zero, bad shifts, array bounds, dead branches.

**Usage**:
.. code-block:: bash

   ./build/bin/lotus-kint -check-all <input.ll>
   ./build/bin/lotus-kint -check-int-overflow -check-array-oob <input.ll>
