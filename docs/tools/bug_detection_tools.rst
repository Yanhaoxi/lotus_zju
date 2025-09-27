Bug Detection Tools
=====================

Static analysis tools for detecting various types of bugs and vulnerabilities.

Kint Static Bug Finder
-----------------------

**Binary**: ``build/bin/kint``

Static bug finder for integer-related and taint-style bugs.

**Usage**:
.. code-block:: bash

   ./build/bin/kint [options] <input IR file>

**Bug Checkers**:
* ``-check-all``: Enable all checkers
* ``-check-int-overflow``: Integer overflow detection
* ``-check-div-by-zero``: Division by zero detection
* ``-check-bad-shift``: Invalid shift detection
* ``-check-array-oob``: Array out of bounds detection
* ``-check-dead-branch``: Dead branch detection

**Performance Options**:
* ``-function-timeout=<seconds>``: Timeout per function (default: 10)
* ``-log-level=<level>``: ``debug``, ``info``, ``warning``, ``error``, ``none``
* ``-quiet``: Suppress most output
* ``-log-to-file=<filename>``: Redirect logs to file

**Examples**:
.. code-block:: bash

   ./build/bin/kint -check-all example.ll
   ./build/bin/kint -check-int-overflow -check-div-by-zero example.ll
   ./build/bin/kint -function-timeout=30 -log-level=debug example.ll

Null Pointer Analysis Tool
--------------------------

**Binary**: ``build/bin/canary-npa``

Context-sensitive null pointer analysis.

**Usage**:
.. code-block:: bash

   ./build/bin/canary-npa [options] <input bitcode file>

**Options**:
* ``-verbose``: Verbose output
* ``-dump-stats``: Print statistics
* ``-context-sensitive``: Enable context-sensitive analysis

**Example**:
.. code-block:: bash

   ./build/bin/canary-npa -verbose -dump-stats example.bc

Buffer Overflow Detection
-------------------------

Integrated into Kint through array out of bounds checker.

**Usage**:
.. code-block:: bash

   ./build/bin/kint -check-array-oob example.ll

**Detection**: Array bounds, buffer overruns, string vulnerabilities

Memory Safety Analysis
----------------------

**Tools**:
* Null Pointer Analysis: ``canary-npa``
* Buffer Overflow: ``kint -check-array-oob``
* Use-After-Free: Through alias analysis
* Memory Leaks: Through value flow analysis

**Comprehensive Analysis**:
.. code-block:: bash

   ./build/bin/canary-npa example.bc
   ./build/bin/kint -check-array-oob example.ll
   ./build/bin/canary-gvfa -vuln-type=nullpointer example.bc

Bug Detection Workflow
----------------------

1. **Compile to LLVM IR**:
   .. code-block:: bash

      clang -emit-llvm -c example.c -o example.bc
      clang -emit-llvm -S example.c -o example.ll

2. **Run Analysis**:
   .. code-block:: bash

      ./build/bin/kint -check-all example.ll
      ./build/bin/canary-npa example.bc
      ./build/bin/ifds-taint example.bc

3. **Review Results**: Check vulnerabilities, validate findings, prioritize fixes

Output Examples
---------------

**Integer Overflow**:
.. code-block:: text

   Bug Report:
   Function: main
   Location: example.c:15:5
   Type: Integer Overflow
   Severity: HIGH
   Code: result = a + b;

**Null Pointer**:
.. code-block:: text

   Null Pointer Vulnerability:
   Function: process_data
   Location: example.c:25:10
   Type: Null Pointer Dereference
   Severity: CRITICAL
   Code: *ptr = value;

Performance
-----------

* Small programs (< 1K functions): < 5 seconds
* Medium programs (1K-10K functions): 5-30 seconds
* Large programs (> 10K functions): 30+ seconds

**Tips**: Use timeouts, appropriate logging, precision trade-offs

Tool Selection
--------------

| Bug Type | Tool | Options |
|----------|------|---------|
| Integer Bugs | Kint | ``-check-int-overflow``, ``-check-div-by-zero`` |
| Memory Bugs | canary-npa, Kint | ``-check-array-oob`` |
| Security Bugs | ifds-taint, canary-gvfa | Various options |
| Performance Bugs | Kint | ``-check-dead-branch`` |