# FPSolve Parser Migration

## Overview

The FPSolve parser has been successfully migrated from `~/Downloads/FPsolve-master/c/src/` to this directory. The parser uses Boost.Spirit for parsing grammar equations over various semiring types.

## Files Migrated

### Core Parser Files

- **`Parser.cpp`** (263 lines) - Full Boost.Spirit parser implementation
- **`include/Solvers/FPSolve/Parser/Parser.h`** - Parser interface

### Supporting Files
- **`GrammarChecker.cpp`** (250 lines) - Grammar equivalence checker
- **`include/Solvers/FPSolve/Utils/MatrixFreeSemiring.h`** - Matrix evaluation utilities

### Example and Test Files
- **`examples/simplegrammar.g`** - Simple recursive grammar example
- **`examples/lossy_stress.g`** - Complex stress test grammar
- **`examples/README.md`** - Grammar format documentation
- **`scripts/approximate.sh`** - Benchmarking script
- **`scripts/convert.sh`** - Graphviz conversion utility

## Dependencies

### Required
- **Boost.Spirit** (header-only) - For grammar parsing
- **Boost.Phoenix** (header-only) - For parser semantic actions
- **Boost.Fusion** (header-only) - For data structure handling

### Optional
- **Boost.Program_Options** - For command-line tools (GrammarChecker)

## Supported Semiring Types

The parser currently supports:

1. **CommutativeRExp** ✅
   - Regular expressions over commutative semirings
   - Format: `<X> ::= "a" <Y> | "b";`

2. **FreeSemiring** ✅
   - Free symbolic expressions
   - Format: `<X> ::= a <Y><Y> | c;`

3. **SemilinSetNdd** ✅ (conditional, requires USE_GENEPI)
   - Semilinear sets using NDDs
   - Format: `<X> ::= "<a:1, b:2>" <Y> | "<>";`

4. **PrefixSemiring** ⚠️ (disabled, requires HAVE_PREFIX_SEMIRING)
   - Prefix sequences
   - Format: `<X> ::= "a,b," <Y> | "c,";`
   - Not migrated yet, guarded by `#ifdef HAVE_PREFIX_SEMIRING`

## Usage Example

```cpp
#include "Solvers/FPSolve/Parser/Parser.h"
#include "Solvers/FPSolve/Semirings/FreeSemiring.h"

// Create parser
fpsolve::Parser p;

// Parse a grammar string
std::string grammar = "<X> ::= a <X><X> | c;";
auto equations = p.free_parser(grammar);

// Process equations
for (const auto& [var, poly] : equations) {
    std::cout << "Variable: " << var << std::endl;
    // Use polynomial...
}
```

## Grammar Format

### Variables
- Enclosed in angle brackets: `<X>`, `<Y>`, `<StartSymbol>`

### Terminals
- Enclosed in double quotes: `"a"`, `"terminal"`, `"123"`

### Productions
- Format: `<LHS> ::= <RHS> ;`
- Alternatives separated by `|`
- Concatenation by juxtaposition

### Example
```
<S> ::= "a" <S> "b" | <C> <C> | "f";
<C> ::= "c" <D> | "b" <S>;
<D> ::= <C> "a" <D> | "b" <D> "e";
```

## Conditional Compilation

### PrefixSemiring Support
To enable PrefixSemiring support:
1. Migrate `semirings/prefix-semiring.h/cpp` from FPsolve-master
2. Add to CMakeLists.txt
3. Define `HAVE_PREFIX_SEMIRING` preprocessor flag

### Genepi/NDD Support
To enable SemilinSetNdd support:
1. Install genepi library
2. Migrate `semirings/semilinSetNdd.h/cpp`
3. Define `USE_GENEPI` preprocessor flag

## Integration with CMake

The parser is included in the FPSolve library build:

```cmake
set(FPSolve_SRCS
  ...
  Parser.cpp
  ...
)

# Boost is auto-detected
find_package(Boost COMPONENTS program_options QUIET)
```

## Known Limitations

1. **PrefixSemiring**: Not yet available (requires additional migration)
2. **SemilinSetNdd**: Requires external genepi library
3. **Boost Dependency**: Requires Boost headers to be installed
4. **C++11**: Requires modern C++ compiler

## Testing

Test the parser with example grammars:

```bash
cd lib/Solvers/FPSolve/examples
cat simplegrammar.g
cat lossy_stress.g
```

## Troubleshooting

### Missing Boost Headers
```
error: boost/spirit/include/qi.hpp: No such file or directory
```
**Solution**: Install Boost development headers
```bash
# Ubuntu/Debian
sudo apt-get install libboost-dev

# macOS
brew install boost
```

### PrefixSemiring Undefined
```
error: 'PrefixSemiring' was not declared in this scope
```
**Solution**: This is expected. PrefixSemiring support is disabled by default.
To enable, migrate the PrefixSemiring files and define `HAVE_PREFIX_SEMIRING`.

### CommutativeRExp Missing
```
error: no matching function for call to 'CommutativeRExp::CommutativeRExp'
```
**Solution**: Ensure `CommutativeRExp.cpp` is built and linked properly.
Check that `lib/Solvers/FPSolve/CMakeLists.txt` includes it.

## See Also

- **Main migration status**: `/FPSOLVE_MIGRATION_STATUS.md`
- **Examples**: `examples/README.md`
- **Original source**: `~/Downloads/FPsolve-master/c/src/parser.cpp`
- **Grammar checker**: `GrammarChecker.cpp`

## Future Work

Potential improvements:
1. Migrate PrefixSemiring support
2. Add more comprehensive test cases
3. Improve error messages for parse failures
4. Add support for reading from files directly
5. Create standalone parser executable

