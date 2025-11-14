Utility Libraries
================

General-purpose utilities and LLVM-specific helpers.

General Utilities
----------------

Core utility functions and data structures.

**Location**: ``lib/Utils/General/``

**ADT (Abstract Data Types)**:
* **BitVector** – Efficient bit set operations
* **Interval** – Range and interval arithmetic
* **Worklist** – Work queue implementations
* **Graph** – Generic graph data structures

**IO Utilities**:
* **FileUtils** – File system operations
* **StringUtils** – String manipulation functions
* **ProgressBar** – Progress reporting utilities

**Debug and Profiling**:
* **Debug.h** – Debug output and assertions
* **Log.h** – Logging infrastructure
* **Timer.h** – Performance timing utilities
* **Statistics.h** – Analysis statistics collection

**Usage**:
.. code-block:: cpp

   #include <Utils/BitVector.h>
   BitVector bv(100);
   bv.set(42);
   bool isSet = bv.test(42);

   #include <Utils/Timer.h>
   Timer timer;
   timer.start();
   // ... operations ...
   timer.stop();
   double elapsed = timer.getElapsedTime();

LLVM Utilities
--------------

LLVM-specific utility functions and helpers.

**Location**: ``lib/Utils/LLVM/``

**IR Manipulation**:
* **IRBuilder helpers** – Extended IR construction utilities
* **Type utilities** – Type system helpers
* **Pass infrastructure** – Analysis and transform pass utilities

**Analysis Helpers**:
* **Alias analysis adapters** – AA integration utilities
* **Dominator utilities** – Dominator tree helpers
* **Loop analysis** – Loop structure utilities

**Code Generation**:
* **Code emission helpers** – LLVM code generation utilities
* **Optimization helpers** – Transform coordination utilities

**Usage**:
.. code-block:: cpp

   #include <Utils/LLVMUtils.h>
   // LLVM-specific utility functions
   bool isVolatileLoad = LLVMUtils::isVolatileLoad(instruction);
