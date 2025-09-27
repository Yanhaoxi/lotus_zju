Alias Analysis Tools
=====================

Command-line tools for alias analysis in Lotus.

DyckAA Tool (canary)
--------------------

**Binary**: ``build/bin/canary``

Unification-based alias analysis tool.

**Usage**:
.. code-block:: bash

   ./build/bin/canary [options] <input bitcode file>

**Key Options**:
* ``-print-alias-set-info``: Print alias sets in DOT format
* ``-count-fp``: Count function pointer targets
* ``-dot-dyck-callgraph``: Generate call graph visualization
* ``-no-function-type-check``: Disable function type checking

**Examples**:
.. code-block:: bash

   ./build/bin/canary -print-alias-set-info example.bc
   ./build/bin/canary -dot-dyck-callgraph -with-labels example.bc

OriginAA Tool
-------------

**Binary**: ``build/bin/origin_aa``

K-callsite sensitive and origin-sensitive pointer analysis.

**Usage**:
.. code-block:: bash

   ./build/bin/origin_aa [options] <input bitcode file>

**Key Options**:
* ``-analysis-mode=<mode>``: ``ci``, ``kcs``, ``origin``
* ``-k=<N>``: Set k value for k-callsite-sensitive analysis
* ``-taint``: Enable taint analysis
* ``-print-cg``, ``-print-pts``, ``-print-tainted``: Output files

**Examples**:
.. code-block:: bash

   ./build/bin/origin_aa -analysis-mode=kcs -k=2 example.bc
   ./build/bin/origin_aa -analysis-mode=origin -taint example.bc

FPA Tool
--------

**Binary**: ``build/bin/fpa``

Function Pointer Analysis with multiple algorithms.

**Usage**:
.. code-block:: bash

   ./build/bin/fpa [options] <input bitcode files>

**Key Options**:
* ``-analysis-type=<N>``: ``1``=FLTA, ``2``=MLTA, ``3``=MLTADF, ``4``=KELP
* ``-max-type-layer=<N>``: Set maximum type layer for MLTA
* ``-output-file=<path>``: Output file path

**Examples**:
.. code-block:: bash

   ./build/bin/fpa -analysis-type=1 example.bc  # FLTA
   ./build/bin/fpa -analysis-type=2 -output-file=results.txt example.bc

DynAA Tools
-----------

**Binaries**: ``build/bin/dynaa-instrument``, ``build/bin/dynaa-check``, ``build/bin/dynaa-log-dump``

Dynamic validation of static analysis results.

**Workflow**:
.. code-block:: bash

   # 1. Instrument
   ./build/bin/dynaa-instrument example.bc -o example.inst.bc
   
   # 2. Compile and run
   clang example.inst.bc libRuntime.a -o example.inst
   LOG_DIR=logs/ ./example.inst
   
   # 3. Check results
   ./build/bin/dynaa-check example.bc logs/pts.log basic-aa

Sea-DSA Tools
-------------

**Binaries**: ``build/bin/sea-dsa-dg``, ``build/bin/seadsa-tool``

Memory graph generation and analysis.

**Usage**:
.. code-block:: bash

   ./build/bin/sea-dsa-dg --sea-dsa-dot example.bc
   ./build/bin/seadsa-tool --sea-dsa-dot --outdir results/ example.bc

Tool Comparison
---------------

| Tool | Precision | Performance | Best For |
|------|-----------|-------------|----------|
| canary | High | Moderate | General analysis |
| origin_aa | Very High | Moderate | Precise analysis |
| fpa | High | Good | Function pointers |
| dynaa-* | Runtime | Runtime | Validation |
| sea-dsa-* | High | Good | Memory analysis |
