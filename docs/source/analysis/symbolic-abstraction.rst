SymbolicAbstraction – Symbolic Abstraction Framework
==========================================

A framework for static program analysis using symbolic abstraction  on LLVM IR.

**Headers**: ``include/Analysis/SymbolicAbstraction``

**Implementation**: ``lib/Analysis/SymbolicAbstraction``

**Main components**:

- **Analyzer** – Fixpoint engine that drives abstract interpretation over a function
- **FragmentDecomposition** – Partitions CFG into acyclic fragments for scalable analysis
- **DomainConstructor** – Factory for creating and composing abstract domains
- **FunctionContext** – Per-function analysis context and state management
- **ModuleContext** – Module-level context for interprocedural setup
- **SymbolicAbstractionPass** – LLVM function pass that integrates SymbolicAbstraction into optimization pipelines
- **AbstractValue** – Base interface for abstract domain values
- **InstructionSemantics** – Converts LLVM instructions to SMT expressions

**Abstract Domains** (in ``domains/``):

- **NumRels** – Numerical relations (e.g., ``x <= y + 5``)
- **Intervals** – Value range analysis (e.g., ``x ∈ [0, 100]``)
- **Affine** – Affine relationships (e.g., ``y = 2*x + 3``)
- **BitMask** – Bit-level tracking and alignment
- **SimpleConstProp** – Constant propagation
- **Boolean** – Boolean truth values and invariants
- **Predicates** – Path predicates and assertions
- **MemRange** – Memory access bounds in terms of function arguments
- **MemRegions** – Memory region and pointer analysis

**Typical use cases**:

- Constant propagation and dead code elimination
- Bounds checking and array access verification
- Bit-level analysis and alignment tracking
- Numerical invariant discovery
- Memory safety analysis
- Custom abstract interpretation passes

**Basic usage (C\+\+)**:

.. code-block:: cpp

   #include <Analysis/SymbolicAbstraction/SymbolicAbstractionPass.h>
   #include <Analysis/SymbolicAbstraction/Analyzer.h>
   #include <Analysis/SymbolicAbstraction/FragmentDecomposition.h>
   #include <Analysis/SymbolicAbstraction/DomainConstructor.h>

   // Using SymbolicAbstractionPass as an LLVM pass
   llvm::Function &F = ...;
   symbolic_abstraction::SymbolicAbstractionPass pass;
   pass.runOnFunction(F);

   // Or using the Analyzer directly
   auto mctx = std::make_unique<symbolic_abstraction::ModuleContext>(F.getParent(), config);
   auto fctx = mctx->createFunctionContext(&F);
   auto fragments = symbolic_abstraction::FragmentDecomposition::For(*fctx, 
       symbolic_abstraction::FragmentDecomposition::Headers);
   symbolic_abstraction::DomainConstructor domain = /* construct domain */;
   auto analyzer = symbolic_abstraction::Analyzer::New(*fctx, fragments, domain);
   analyzer->run();

   // Query results
   llvm::BasicBlock *BB = ...;
   const symbolic_abstraction::AbstractValue *state = analyzer->at(BB);

**Fragment Strategies**:

- **Edges** – Abstract after every basic block (most precise, slowest)
- **Function** – Analyze whole function as one fragment (fastest, least precise)
- **Headers** – Place abstraction points at loop headers (good balance)
- **Body** – Abstract in loop bodies
- **Backedges** – Abstract at loop backedges

**Integration**:

SymbolicAbstraction can be used as:

- An LLVM ``FunctionPass`` via ``SymbolicAbstractionPass``
- A standalone analysis library via ``Analyzer``
- A tool via ``build/bin/symbolic_abstraction`` (see :doc:`../tools/verifier/symbolic_abstraction/index`)

For more details on using SymbolicAbstraction as a tool, see :doc:`../tools/verifier/symbolic_abstraction/index`.


