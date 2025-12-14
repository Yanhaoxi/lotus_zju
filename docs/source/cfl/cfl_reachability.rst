CFL Reachability Analysis
==========================

Context-Free Language (CFL) reachability extends graph reachability with context-free grammars for precise interprocedural analysis.

Overview
--------

CFL reachability enables analysis of complex program properties using grammar-based constraints.

**Location**: ``tools/cfl/``

**Tools**: Graspan (disk-based, parallel reachability), CSR (indexed CFL reachability)

Graspan
-------

Disk-based parallel CFL reachability for large graphs.

**Usage**:
.. code-block:: bash

   ./build/bin/graspan <graph> <grammar> <partitions> <memory_gb> <threads>

**Input**: Edge-list graph files, CNF grammar files
**Output**: Partitioned graphs with transitive closure

CSR (Context-Sensitive Reachability)
------------------------------------

Indexing-based reachability with multiple algorithms.

**Algorithms**: GRAIL, PathTree, Combined

**Usage**:
.. code-block:: bash

   ./build/bin/csr [options] <graph>
   ./build/bin/csr -m grail -p -j 8 input.graph  # Parallel GRAIL

**Options**: ``-m <method>``, ``-n <queries>``, ``-j <threads>``, ``-t`` (transitive closure)


