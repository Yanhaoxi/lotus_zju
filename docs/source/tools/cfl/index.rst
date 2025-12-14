CFL Tools
=========

This page documents the CFL-related tools under ``tools/cfl/``. For the
underlying theory and components, see :doc:`../../analysis/cfl_reachability`.

CSR: Context-Sensitive Reachability
-----------------------------------

Indexing-based context-sensitive reachability engine for large graphs.

**Binary**: ``csr``  
**Location**: ``tools/cfl/csr/csr.cpp``

CSR operates on graph files (not LLVM bitcode directly) and answers reachability
queries with different indexing strategies (GRAIL, PathTree, or combined).

**Basic Usage**:

.. code-block:: bash

   ./build/bin/csr [options] graph_file

**Common Options** (see ``tools/cfl/csr/README.md`` for full list):

- ``-m <method>`` – Indexing method:

  - ``pathtree`` – PathTree indexing
  - ``grail`` – GRAIL labeling
  - ``pathtree+grail`` – Combined approach

- ``-t`` – Evaluate transitive closure
- ``-r`` – Evaluate tabulation algorithm
- ``-p`` – Evaluate parallel tabulation algorithm
- ``-j <N>`` – Number of threads for parallel tabulation (0 = auto)
- ``-g <file>`` – Generate queries and save to file
- ``-q <file>`` – Load queries from file

**Examples**:

.. code-block:: bash

   # GRAIL-based reachability
   ./build/bin/csr input.graph

   # PathTree indexing
   ./build/bin/csr -m pathtree input.graph

   # Parallel tabulation with 4 threads
   ./build/bin/csr -p -j 4 input.graph

Graspan: Disk-Based Parallel Graph System
-----------------------------------------

Graspan is a disk-based parallel graph engine for computing dynamic transitive
closure on large program graphs. Lotus bundles the C++ implementation.

**Binary**: ``graspan``  
**Location**: ``tools/cfl/graspan/graspan.cpp``

Graspan operates on edge-list graphs and grammar files describing CFL rules.

**Input Format**:

- **Graph file** (edge list):

  .. code-block:: text

     <src> <dst> <label>

- **Grammar file** (CNF rules):

  .. code-block:: text

     A B C   # A → B C

**Basic Usage**:

.. code-block:: bash

   ./build/bin/graspan <graph_file> <grammar_file> <num_partitions> <mem_gb> <num_threads>

See ``tools/cfl/graspan/README.md`` and the original Graspan documentation for
details on configuration and performance tuning.
