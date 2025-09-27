Intermediate Representations
==============================

Lotus provides several intermediate representations for program analysis.

Program Dependence Graph (PDG)
-------------------------------

Captures data and control dependencies in programs.

**Location**: ``lib/IR/PDG``

**Features**:
* Data and control dependence edges
* Memory dependence via alias analysis
* Program slicing support

**Usage**:
.. code-block:: cpp

   #include <IR/PDG/ProgramDependenceGraph.h>
   auto pdg = std::make_unique<ProgramDependenceGraph>(function, aliasAnalysis);

Value Flow Graph (VFG)
-----------------------

Tracks value flow through programs, including memory operations.

**Types**:
* **DyckVFG**: Integrated with DyckAA
* **SVFG**: Sparse Value Flow Graph (planned)

**Applications**: Null pointer analysis, memory safety, taint analysis

**Usage**:
.. code-block:: cpp

   #include <Alias/DyckAA/DyckVFG.h>
   auto vfg = dyckAA->getValueFlowGraph();

Call Graph
----------

Captures function call relationships.

**Types**:
* Static call graph (direct calls)
* Dynamic call graph (indirect calls)
* Context-sensitive call graph

**Visualization**:
.. code-block:: bash

   ./build/bin/canary -dot-dyck-callgraph example.bc

Memory Graph (Sea-DSA)
----------------------

Detailed memory usage representation from Sea-DSA.

**Usage**:
.. code-block:: bash

   ./build/bin/sea-dsa-dg --sea-dsa-dot example.bc
   ./build/bin/seadsa-tool --sea-dsa-dot --outdir results/ example.bc

**Output**: DOT files, memory layout, pointer relationships

Analysis Pipeline
-----------------

1. Load LLVM module
2. Run alias analysis
3. Build intermediate representation
4. Perform analysis on IR
5. Generate results

**Example**:
.. code-block:: cpp

   auto module = loadLLVMModule("example.bc");
   auto aliasAnalysis = runDyckAA(module);
   auto pdg = buildPDG(module, aliasAnalysis);
   auto slice = pdg->computeSlice(targetInstruction);
