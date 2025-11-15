Analysis Components
===================

Lotus provides several reusable analysis utilities and frameworks under ``lib/Analysis``.
These components complement the alias analyses and high-level analyzers such as
CLAM (numerical abstract interpretation) and Sprattus (symbolic abstraction).

This page gives a high-level map of the analysis infrastructure. Each
subdirectory under ``lib/Analysis`` has a dedicated page with more detail.

Overview
--------

At a glance:

- **CFG** (``lib/Analysis/CFG``): Control Flow Graph utilities for reachability,
  dominance, and structural reasoning. See :doc:`cfg`.
- **Concurrency** (``lib/Analysis/Concurrency``): Thread-aware analyses for
  multi-threaded code (MHP, lock sets, thread modeling). See :doc:`concurrency`.
- **GVFA** (``lib/Analysis/GVFA``): Global value-flow engine for interprocedural
  data-flow reasoning. See :doc:`gvfa`.
- **NullPointer** (``lib/Analysis/NullPointer``): A family of nullness and
  null-flow analyses. See :doc:`null_pointer`.

Higher-level analyzers such as CLAM and Sprattus build on these components;
see :doc:`numerical_abstract_interpretation` and :doc:`symbolic_abstraction`
for details.

