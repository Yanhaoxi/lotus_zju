# FPSolve Grammar Examples

This directory contains example grammar files for testing FPSolve.

## Grammar Format

FPSolve uses a simple grammar notation:

```
<Variable> ::= "terminal" <Variable> | "other";
```

- **Variables**: Enclosed in angle brackets `<X>`, `<Y>`, etc.
- **Terminals**: Enclosed in double quotes `"a"`, `"b"`, etc.
- **Alternatives**: Separated by pipe `|`
- **Concatenation**: Juxtaposition of symbols
- **Productions**: End with semicolon `;`

## Example Files

### simplegrammar.g

A simple recursive grammar demonstrating nested structure:

```
<S> ::= "s"<A><A> | "()";
<A> ::= "s"<B><B>;
...
<H> ::= "s"<S><S>;
```

This creates a deeply nested recursive structure useful for stress testing the solver.

### lossy_stress.g

A more complex grammar with multiple production rules:

```
<S> ::= "a" <S> "b" | <C> <C> | "f";
<C> ::= "c" <D> | "b" <S> ;
<D> ::= <C> "a" <D> | "b" <D> "e" | <C> "b" <S> "a" <C> "c";
```

This grammar is designed to test lossy approximation and subword closure algorithms.

## Usage

To parse a grammar file, use the Parser class:

```cpp
#include "Solvers/FPSolve/Parser/Parser.h"

fpsolve::Parser p;
auto equations = p.free_parser(read_file("examples/simplegrammar.g"));
```

## Semirings

The parser can work with different semirings:

- **FreeSemiring**: Free symbolic expressions
- **CommutativeRExp**: Regular expressions (commutative)
- **BoolSemiring**: Boolean algebra
- **TropicalSemiring**: Min-plus algebra
- **LossyFiniteAutomaton**: Lossy automata (requires libfa)
- **SemilinSetNdd**: Semilinear sets with NDDs (requires genepi)

## See Also

- `lib/Solvers/FPSolve/MainExample.cpp` - Example CLI usage
- `lib/Solvers/FPSolve/GrammarChecker.cpp` - Grammar equivalence checking
- Original FPsolve documentation: `~/Downloads/FPsolve-master/README.md`

