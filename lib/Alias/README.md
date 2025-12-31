# The Phenix Alias Analsyis Toolkits

This directory contains various alias analysis implementations and toolkits used in the Phenix project.


- SparrowAA: inclusion-based, flow-insensitive, context-insensitive, and context-sensitive pointer analsyis (ci, 1-cfa, 2-cfa)
- AserPTA: inclusion-based, field-sensitive, flow-insensitive, context-insensitive, and context-sensitive pointer analsyis (ci, 1-cfa, 2-cfa, origin).
- CFLAA: flow-insensitive, context-insensitive pointer analysis (from LLVM).
- DyckAA: unification-based, flow-insensitive, context-insensitive pointer analysis. 
- Dynamic: dynamic pointer analysis.
- FPA: type-based function pointer analysis.
- LotusAA: inclusion-based,  flow- and context-sensitive pointer analysis.
- seadsa: unificaiton-based, flow-insensitive, context-sensitive  pointer analysis (with heap cloning, a.k.a., context-sensitive heap).
- SRAA: range-based alias analsyis (flow- and context-insensitive)
- UnderApproxAA: pattern-based must-alias analysis
- AllocAA.

Note: SparrowAA and AserPTA have some redundancies, but are different
- SparrowAA does not use on-the-fly callgraph construction, while AserPTA uses it by default.
- SparrowAA integrates more graph simplification algorithms
- SparrowAA is filed-insensitive (ConstraintCollect.cpp, NodeFactory.cpp). AserPTA supports both filed-insensitive and filed-sensitive mode.
- SparrowAA supports CI, 1-CFA, 2-CFA, whereas AserPTA has one more "orgin-sensitive" variant
