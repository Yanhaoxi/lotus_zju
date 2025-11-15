=====================================
FPA — Function Pointer Analyses (FLTA/MLTA/MLTADF/KELP)
=====================================

Overview
========

The FPA module implements several **function pointer analysis** algorithms to
resolve indirect calls with different precision/performance trade-offs.

* **Location**: ``lib/Alias/FPA``
* **Focus**: Indirect call resolution and call-graph construction
* **Algorithms**:
  - **FLTA** – Flow-insensitive, type-based analysis
  - **MLTA** – Multi-layer type analysis
  - **MLTADF** – MLTA with additional data-flow reasoning
  - **KELP** – Context-sensitive analysis (USENIX Security'24)

Workflow
========

All FPA variants share a common high-level structure:

1. Scan the program to collect function pointer definitions and uses.
2. Build an abstract model of **types**, **call sites**, and **targets**.
3. Apply the selected algorithm (1–4) to approximate the mapping from call
   sites to possible function targets.
4. Optionally emit diagnostic or visualization output (e.g., call graphs).

Usage
=====

The analyses are exposed through the ``fpa`` driver:

.. code-block:: bash

   ./build/bin/fpa -analysis-type=1 example.bc      # FLTA
   ./build/bin/fpa -analysis-type=2 example.bc      # MLTA
   ./build/bin/fpa -analysis-type=3 example.bc      # MLTADF
   ./build/bin/fpa -analysis-type=4 example.bc      # KELP

Useful options:

* ``-analysis-type=<N>`` – choose algorithm (1–4).
* ``-max-type-layer=<N>`` – maximum type depth for MLTA/MLTADF.
* ``-debug`` – enable debugging output.
* ``-output-file=<path>`` – path for analysis results.

FPA results can be consumed directly (for security analyses or refactoring)
or fed into other components that benefit from precise indirect call
resolution.


