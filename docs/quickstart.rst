Quick Start Guide
==================

Get up and running with Lotus quickly.

Basic Usage
-----------

Compile your C/C++ code to LLVM IR:

.. code-block:: bash

   clang -emit-llvm -c example.c -o example.bc
   clang -emit-llvm -S example.c -o example.ll

Alias Analysis
--------------

.. code-block:: bash

   ./build/bin/canary example.bc                    # Basic analysis
   ./build/bin/fpa example.bc                       # Function pointers
   ./build/bin/origin_aa example.bc                 # Context-sensitive

Taint Analysis
--------------

.. code-block:: bash

   ./build/bin/ifds-taint example.bc                # Basic taint analysis
   ./build/bin/ifds-taint -sources="read,scanf" -sinks="system,exec" example.bc

Bug Detection
-------------

.. code-block:: bash

   ./build/bin/kint -check-int-overflow example.ll  # Integer overflow
   ./build/bin/canary-npa example.bc                # Null pointer
   ./build/bin/kint -check-all example.ll           # All checks

Memory Analysis
---------------

.. code-block:: bash

   ./build/bin/sea-dsa-dg --sea-dsa-dot example.bc
   ./build/bin/seadsa-tool --sea-dsa-dot --outdir results/ example.bc

Dynamic Validation
------------------

.. code-block:: bash

   ./build/bin/dynaa-instrument example.bc -o example.inst.bc
   clang example.inst.bc libRuntime.a -o example.inst
   LOG_DIR=logs/ ./example.inst
   ./build/bin/dynaa-check example.bc logs/pts.log basic-aa

Example Analysis
----------------

Analyze a vulnerable C program:

.. code-block:: c

   // example.c
   #include <stdio.h>
   
   void vulnerable_function(char* input) {
       char buffer[100];
       strcpy(buffer, input);  // Potential buffer overflow
       printf("%s", buffer);
   }
   
   int main() {
       char user_input[200];
       scanf("%s", user_input);  // Source of tainted data
       vulnerable_function(user_input);
       return 0;
   }

Analysis commands:

.. code-block:: bash

   clang -emit-llvm -c example.c -o example.bc
   ./build/bin/ifds-taint example.bc                # Detect taint flow
   ./build/bin/kint -check-array-oob example.ll     # Check buffer overflow
   ./build/bin/sea-dsa-dg --sea-dsa-dot example.bc  # Memory analysis
