# The Phenix Alias Analsyis Toolkits

This directory contains various alias analysis implementations and toolkits used in the Phenix project.


- SparrowAA: inclusion-based, context-insensitive, and context-sensitive pointer analsyis (ci, 1-cfa, 2-cfa)
- AserPTA: inclusion-based, context-insensitive, and context-sensitive pointer analsyis (ci, 1-cfa, 2-cfa, origin).
- CFLAA: flow-insensitive, context-insensitive pointer analysis (from LLVM).
- DyckAA: unification-based, flow-insensitive, context-insensitive pointer analysis. 
- Dynamic: dynamic pointer analysis.
- FPA: type-based function pointer analysis.
- LotusAA: inclusion-based,  flow- and context-sensitive pointer analysis.
- seadsa: unificaiton-based, flow-insensitive, context-sensitive  pointer analysis.
- SRAA: range-based alias analsyis (flow- and context-insensitive)
- UnderApproxAA: must-alias analysis
- AllocAA.

Note: SparrowAA and AserPTA have some redundancies. 