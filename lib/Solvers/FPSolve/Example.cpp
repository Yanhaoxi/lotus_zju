/**
 * FPSolve Example: Demonstrating Newton solver on Boolean semiring
 * 
 * Solves the fixed-point equation:
 *   X = a*X*X + c
 * 
 * Which represents a context-free grammar for binary trees
 */

#include "Solvers/FPSolve/FPSolve.h"
#include <iostream>

using namespace fpsolve;

int main() {
  std::cout << "=== FPSolve Example: Binary Trees ===" << '\n';

  // Create variables
  VarId X = Var::GetVarId("X");
  VarId a = Var::GetVarId("a");
  VarId c = Var::GetVarId("c");

  // Build polynomial: X = a*X*X + c
  // Monomial a*X*X
  CommutativeMonomial m1({X, X});
  // Monomial c (constant)
  CommutativeMonomial m2;

  // Polynomial with Boolean coefficients
  CommutativePolynomial<BoolSemiring> poly({
      {BoolSemiring(true), m1},   // a*X*X (with 'a' as true)
      {BoolSemiring(true), m2}    // c (with 'c' as true)
  });

  // Create equation system
  Equations<BoolSemiring> equations;
  equations.emplace_back(X, poly);

  std::cout << "Equation: X = " << poly.string() << '\n';

  // Solve with Newton method
  std::cout << "\n--- Solving with Newton (LDU) ---" << '\n';
  auto newton_result = SolverFactory<BoolSemiring>::solve(
      equations, 
      10,
      SolverFactory<BoolSemiring>::SolverType::NEWTON_CLDU
  );

  std::cout << "Newton result: X = " << newton_result[X].string() << '\n';

  // Solve with Kleene iteration for comparison
  std::cout << "\n--- Solving with Kleene Iteration ---" << '\n';
  auto kleene_result = SolverFactory<BoolSemiring>::solve(
      equations,
      10,
      SolverFactory<BoolSemiring>::SolverType::KLEENE
  );

  std::cout << "Kleene result: X = " << kleene_result[X].string() << '\n';

  // Example with Tropical semiring (shortest path)
  std::cout << "\n\n=== FPSolve Example: Shortest Path (Tropical) ===" << '\n';
  
  VarId Y = Var::GetVarId("Y");
  
  // Y = 1*Y + 5
  CommutativeMonomial m_y({Y});
  CommutativeMonomial m_const;
  
  CommutativePolynomial<TropicalSemiring> tropical_poly({
      {TropicalSemiring(1), m_y},
      {TropicalSemiring(5), m_const}
  });

  Equations<TropicalSemiring> tropical_eqns;
  tropical_eqns.emplace_back(Y, tropical_poly);

  std::cout << "Equation: Y = " << tropical_poly.string() << '\n';

  auto tropical_result = SolverFactory<TropicalSemiring>::solve(
      tropical_eqns,
      5,
      SolverFactory<TropicalSemiring>::SolverType::NEWTON_CLDU
  );

  std::cout << "Result: Y = " << tropical_result[Y].string() << '\n';

  std::cout << "\n=== Examples Complete ===" << '\n';

  return 0;
}

