Optimization
============

This section covers optimization passes and utilities provided by Lotus.

Lotus provides several optimization passes that are useful for preparing code
for analysis or for improving performance. These passes complement the code
transformations available in :doc:`../transform/index`.

Overview
--------

At a glance:

- **ModuleOptimizer** – Driver pass that runs standard LLVM optimization pipelines (O0, O1, O2, O3)
- **AInliner** – Aggressive inliner tuned for analysis-friendly IR
- **LICM** – Loop Invariant Code Motion (taken from LLVM 14)
- **SWPrefetching** – Software prefetching pass for indirect memory accesses

.. toctree::
   :maxdepth: 2

   optimization
