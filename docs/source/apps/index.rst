Applications
============

This section covers the high-level applications and tools built on Lotus, organized to match the structure of ``lib/Verification/`` and ``lib/Apps/``.

Components
----------

* **Checker** – Bug detection framework (``lib/Checker/``)
* **CLAM** – Abstract interpretation-based analyzer (``lib/Verification/clam/``)
* **Fuzzing** – Directed greybox fuzzing support (``lib/Apps/Fuzzing/``)
* **MCP** – Model Context Protocol server for call graph analysis (``lib/Apps/MCP/``)
* **SeaHorn** – SMT-based verification framework (``lib/Verification/seahorn/``)

.. toctree::
   :maxdepth: 2

   checker/index
   clam
   fuzzing_support
   mcp
   seahorn
