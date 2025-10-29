# IFDS/IDE Dataflow Analysis

## Writing An Analysis

*Use IFDS**, if
- your analysis is a plain reachability problem, that is, a data-flow fact can either hold, or not.
- your analysis problem is distributive, that is, within a flow function the reachability of a successor fact may depend on at most one incoming fact.
- your analysis problem is a may-analysis, since IFDS always uses set-union as merge operator

All bit-vector problems fall into this category, including all classical gen-kill problems. Examples are taint analysis, uninitialized-variables analysis, constness analysis, etc.


*Use IDE**, if

- your analysis computes environments that associate the holding data-flow facts with an additional computed value
- OR the set of holding data-flow facts is structured, i.e. some facts subsume other facts
- OR your analysis is a must-analysis, since in contrast to IFDS the merge operator is customizable
- your analysis problem is distributive, that is, within a flow function the reachability a of a successor fact as well as its associated value may depend on at most one incoming fact/value.

Examples are linear constant-propagation, typestate analysis, type analysis, feature-taint analysis, etc.

## Related Work


You may also refer to https://github.com/secure-software-engineering/phasar/wiki/Useful-Literature

- ISSTA 23: Dongjie He, Yujiang Gui, Yaoqing Gao, and Jingling Xue. Reducing the Memory Footprint of
IFDS-Based Data-Flow Analyses using Fine-Grained Garbage Collection.
- ICSE 21: Steven Arzt. Sustainable Solving: Reducing The Memory Footprint of IFDS-Based Data
Flow Analyses Using Intelligent Garbage Collection.
- ASE 20: Dongjie He, Haofeng Li, Lei Wang, Haining Meng, Hengjie Zheng, Jie Liu, Shuangwei Hu,
Lian Li, and Jingling Xue. Performance-Boosting Sparsification of the IFDS Algorithm with
Applications to Taint Analysis.
- TACAS 19: Philipp Dominik Schubert, Ben Hermann, and Eric Bodden. PhASAR: An Inter-procedural
Static Analysis Framework for C/C++.
- ECOOP 16: Johannes Späth, Lisa Nguyen Quang Do, Karim Ali, and Eric Bodden. Boomerang: DemandDriven Flow- and Context-Sensitive Pointer Analysis for Java.
- ICSE 16: Steven Arzt and Eric Bodden. StubDroid: Automatic Inference of Precise Data-Flow Summaries for the Android Framework.
- ICSE 15: Cathrin Weiss, Cindy Rubio-González, and Ben Liblit. Database-backed program analysis for
scalable error propagation.
- ICSE 14: Steven Arzt and Eric Bodden. Reviser: Efficiently Updating IDE-/IFDS-Based Data-Flow Analyses in Response to Incremental Program Changes.
- PLDI 14: Steven Arzt, Siegfried Rasthofer, Christian Fritz, Eric Bodden, Alexandre Bartel, Jacques
Klein, Yves Le Traon, Damien Octeau, and Patrick D. McDaniel. FlowDroid: precise context,
flow, field, object-sensitive and lifecycle-aware taint analysis for Android app.
- CC 10: Nomair A Naeem, Ondřej Lhoták, and Jonathan Rodriguez. Practical Extensions to the IFDS
Algorithm.
- CC 08: Atanas Rountev, Mariana Sharp, and Guoqing Xu. IDE Dataflow Analysis in the Presence of
Large Object-Oriented Libraries.
- CAV 05: Akash Lal, Thomas Reps, and Gogul Balakrishnan. Extended Weighted Pushdown Systems.
- SAS 03: Thomas Reps, Stefan Schwoon, and Somesh Jha. Weighted Pushdown Systems and Their
Application to Interprocedural Dataflow Analysis
- POPL 95: Thomas Reps, Susan Horwitz, and Mooly Sagiv. Precise Interprocedural Dataflow Analysis via Graph Reachability