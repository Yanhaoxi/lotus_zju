Data Flow Analysis Tools
=========================

Data flow analysis tools for security and program understanding.

IFDS Taint Analysis Tool
-------------------------

**Binary**: ``build/bin/lotus-taint``

Interprocedural taint analysis using IFDS framework.

**Usage**:
.. code-block:: bash

   ./build/bin/lotus-taint [options] <input bitcode file>

**Key Options**:
* ``-analysis=<N>``: ``0``=Taint analysis, ``1``=Reaching definitions
* ``-sources=<functions>``: Custom source functions
* ``-sinks=<functions>``: Custom sink functions
* ``-max-results=<N>``: Limit results shown

**Default Sources**: ``read``, ``scanf``, ``fgets``, ``recv``, ``mmap``

**Default Sinks**: ``system``, ``exec``, ``sprintf``, ``strcpy``

**Examples**:
.. code-block:: bash

   ./build/bin/lotus-taint example.bc
   ./build/bin/lotus-taint -sources="read,scanf" -sinks="system,exec" example.bc
   ./build/bin/lotus-taint -analysis=1 example.bc  # Reaching definitions

Global Value Flow Analysis Tool
-------------------------------

**Binary**: ``build/bin/lotus-gvfa``

Vulnerability detection with null pointer and taint analysis.

**Usage**:
.. code-block:: bash

   ./build/bin/lotus-gvfa [options] <input bitcode file>

**Key Options**:
* ``-vuln-type=<type>``: ``nullpointer``, ``taint``
* ``-test-cfl-reachability``: Context-sensitive analysis
* ``-dump-stats``: Print statistics
* ``-verbose``: Detailed output

**Examples**:
.. code-block:: bash

   ./build/bin/lotus-gvfa example.bc  # Null pointer analysis
   ./build/bin/lotus-gvfa -vuln-type=taint example.bc
   ./build/bin/lotus-gvfa -test-cfl-reachability -verbose example.bc

Analysis Framework
------------------

**IFDS/IDE Framework**: ``include/Analysis/IFDS/``

**Key Components**:
* ``IFDSFramework``: Core implementation
* ``TaintAnalysis``: Taint-specific analysis
* ``ReachingDefsAnalysis``: Reaching definitions

**Usage in Code**:
.. code-block:: cpp

   #include <Analysis/IFDS/IFDSFramework.h>
   auto analysis = std::make_unique<IFDSAnalysis>();
   analysis->setSources(customSources);
   analysis->setSinks(customSinks);
   auto results = analysis->analyze(module);

Analysis Types
--------------

* **Taint Analysis**: Security vulnerability detection
* **Reaching Definitions**: Definition-use chain tracking
* **Value Flow Analysis**: Value flow through memory
* **Null Pointer Analysis**: Null pointer dereference detection

Output Examples
---------------

**Taint Flow**:
.. code-block:: text

   Taint Flow Detected:
   Source: scanf at line 15
   Sink: system at line 25
   Path: scanf -> strcpy -> system
   Severity: HIGH

**Statistics**:
.. code-block:: text

   Analysis Statistics:
   - Functions analyzed: 25
   - Instructions processed: 1,234
   - Taint flows found: 3
   - Analysis time: 2.5 seconds

Performance
-----------

* Small programs (< 1K functions): < 1 second
* Medium programs (1K-10K functions): 1-10 seconds
* Large programs (> 10K functions): 10+ seconds

**Tips**: Use appropriate precision, limit results, consider function-level analysis
