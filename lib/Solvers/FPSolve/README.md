# FPSolve - Fixed-Point Solver for Omega-Continuous Semirings

FPSolve is a fixed-point solver based on Newton's method generalized to omega-continuous semirings. This is a port of the original [FPsolve](https://github.com/mschlund/FPsolve) project, integrated into the Lotus analysis framework.

### Key Equation

For a system of equations **X = F(X)** where **F** is a vector of polynomials, Newton's method iterates:

```
X_{n+1} = X_n + (I - J_F(X_n))^* · δ_n
```

Where:
- `J_F(X_n)` is the Jacobian matrix
- `*` is the Kleene star (closure operation)  
- `δ_n` represents higher-order corrections



## Usage Example

```cpp
#include "Solvers/FPSolve/FPSolve.h"
using namespace fpsolve;

// Create variables
VarId X = Var::GetVarId("X");

// Build polynomial: X = a*X*X + c (binary trees)
CommutativeMonomial m1({X, X});
CommutativeMonomial m2;  // constant

CommutativePolynomial<BoolSemiring> poly({
    {BoolSemiring(true), m1},   // a*X*X
    {BoolSemiring(true), m2}    // c
});

// Create equation system
Equations<BoolSemiring> equations;
equations.emplace_back(X, poly);

// Solve with Newton method
auto result = SolverFactory<BoolSemiring>::solve(
    equations, 
    10,  // max iterations
    SolverFactory<BoolSemiring>::SolverType::NEWTON_CLDU
);

std::cout << "Result: X = " << result[X].string() << std::endl;
```

## Integration with NPA

TBD: integrates with the existing NPA (Newtonian Program Analysis) module of Lotus.
