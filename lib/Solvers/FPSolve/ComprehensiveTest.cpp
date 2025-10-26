/**
 * Comprehensive FPSolve Test Suite
 * Tests all major semirings and solvers
 */

#include "Solvers/FPSolve/FPSolve.h"
#include <iostream>
#include <iomanip>

using namespace fpsolve;

void test_bool_semiring() {
  std::cout << "\n=== Boolean Semiring Test ===" << std::endl;
  
  VarId X = Var::GetVarId("X");
  CommutativeMonomial m1({X, X});
  CommutativeMonomial m_const;
  
  CommutativePolynomial<BoolSemiring> poly({
      {BoolSemiring(true), m1},
      {BoolSemiring(true), m_const}
  });
  
  Equations<BoolSemiring> equations;
  equations.emplace_back(X, poly);
  
  auto result = SolverFactory<BoolSemiring>::solve(
      equations, 5, SolverFactory<BoolSemiring>::SolverType::NEWTON_CLDU
  );
  
  std::cout << "X = " << result[X].string() << std::endl;
}

void test_float_semiring() {
  std::cout << "\n=== Float Semiring Test ===" << std::endl;
  
  VarId Y = Var::GetVarId("Y");
  CommutativeMonomial m_y({Y});
  CommutativeMonomial m_const;
  
  CommutativePolynomial<FloatSemiring> poly({
      {FloatSemiring(0.5), m_y},
      {FloatSemiring(0.3), m_const}
  });
  
  Equations<FloatSemiring> equations;
  equations.emplace_back(Y, poly);
  
  auto result = SolverFactory<FloatSemiring>::solve(
      equations, 10, SolverFactory<FloatSemiring>::SolverType::NEWTON_CLDU
  );
  
  std::cout << "Y = " << std::fixed << std::setprecision(6) 
            << result[Y].getValue() << std::endl;
}

void test_tropical_semiring() {
  std::cout << "\n=== Tropical Semiring Test (Shortest Path) ===" << std::endl;
  
  VarId Z = Var::GetVarId("Z");
  CommutativeMonomial m_z({Z});
  CommutativeMonomial m_const;
  
  CommutativePolynomial<TropicalSemiring> poly({
      {TropicalSemiring(2), m_z},
      {TropicalSemiring(5), m_const}
  });
  
  Equations<TropicalSemiring> equations;
  equations.emplace_back(Z, poly);
  
  auto result = SolverFactory<TropicalSemiring>::solve(
      equations, 5, SolverFactory<TropicalSemiring>::SolverType::KLEENE
  );
  
  std::cout << "Z = " << result[Z].string() << std::endl;
}

void test_viterbi_semiring() {
  std::cout << "\n=== Viterbi Semiring Test ===" << std::endl;
  
  VarId V = Var::GetVarId("V");
  CommutativeMonomial m_v({V});
  CommutativeMonomial m_const;
  
  CommutativePolynomial<ViterbiSemiring> poly({
      {ViterbiSemiring(0.9), m_v},
      {ViterbiSemiring(0.1), m_const}
  });
  
  Equations<ViterbiSemiring> equations;
  equations.emplace_back(V, poly);
  
  auto result = SolverFactory<ViterbiSemiring>::solve(
      equations, 10, SolverFactory<ViterbiSemiring>::SolverType::NEWTON_CLDU
  );
  
  std::cout << "V = " << std::fixed << std::setprecision(6)
            << result[V].getValue() << std::endl;
}

void test_free_semiring() {
  std::cout << "\n=== Free Semiring Test (Symbolic) ===" << std::endl;
  
  VarId a = Var::GetVarId("a");
  VarId b = Var::GetVarId("b");
  
  FreeSemiring fa(a);
  FreeSemiring fb(b);
  
  FreeSemiring expr = fa * fb + fa.star();
  
  std::cout << "Expression: " << expr.string() << std::endl;
  
  // Evaluate with Boolean values
  ValuationMap<BoolSemiring> val_bool;
  val_bool[a] = BoolSemiring(true);
  val_bool[b] = BoolSemiring(true);
  
  auto result_bool = expr.Eval(val_bool);
  std::cout << "Evaluated (bool): " << result_bool.string() << std::endl;
  
  // Evaluate with Float values
  ValuationMap<FloatSemiring> val_float;
  val_float[a] = FloatSemiring(0.5);
  val_float[b] = FloatSemiring(0.3);
  
  auto result_float = expr.Eval(val_float);
  std::cout << "Evaluated (float): " << std::fixed << std::setprecision(6) 
            << result_float.getValue() << std::endl;
}

void test_scc_decomposition() {
  std::cout << "\n=== SCC Decomposition Test ===" << std::endl;
  
  // Create mutually recursive equations
  VarId X = Var::GetVarId("X_scc");
  VarId Y = Var::GetVarId("Y_scc");
  VarId Z = Var::GetVarId("Z_scc");
  
  // X depends on Y
  CommutativePolynomial<BoolSemiring> poly_x({
      {BoolSemiring(true), CommutativeMonomial({Y})}
  });
  
  // Y depends on X (mutual recursion)
  CommutativePolynomial<BoolSemiring> poly_y({
      {BoolSemiring(true), CommutativeMonomial({X})}
  });
  
  // Z is independent
  CommutativePolynomial<BoolSemiring> poly_z({
      {BoolSemiring(true), CommutativeMonomial()}
  });
  
  Equations<BoolSemiring> equations;
  equations.emplace_back(X, poly_x);
  equations.emplace_back(Y, poly_y);
  equations.emplace_back(Z, poly_z);
  
  std::cout << "Solving with SCC decomposition..." << std::endl;
  
  auto result = apply_solver_with_scc<NewtonCLDU, CommutativePolynomial>(
      equations, 10
  );
  
  std::cout << "X = " << result[X].string() << std::endl;
  std::cout << "Y = " << result[Y].string() << std::endl;
  std::cout << "Z = " << result[Z].string() << std::endl;
}

void test_commutative_rexp() {
  std::cout << "\n=== Commutative Regular Expression Test ===" << std::endl;
  
  VarId a = Var::GetVarId("a_rexp");
  VarId b = Var::GetVarId("b_rexp");
  
  CommutativeRExp ra(a);
  CommutativeRExp rb(b);
  
  CommutativeRExp expr = (ra * rb).star() + ra;
  
  std::cout << "Regular Expression: " << expr.string() << std::endl;
}

int main() {
  std::cout << "╔════════════════════════════════════════════════════════════╗" << std::endl;
  std::cout << "║        FPSolve Comprehensive Test Suite                   ║" << std::endl;
  std::cout << "║  Fixed-Point Solver for Omega-Continuous Semirings        ║" << std::endl;
  std::cout << "╚════════════════════════════════════════════════════════════╝" << std::endl;
  
  test_bool_semiring();
  test_float_semiring();
  test_tropical_semiring();
  test_viterbi_semiring();
  test_free_semiring();
  test_commutative_rexp();
  test_scc_decomposition();
  
  std::cout << "\n╔════════════════════════════════════════════════════════════╗" << std::endl;
  std::cout << "║               All Tests Completed Successfully!           ║" << std::endl;
  std::cout << "╚════════════════════════════════════════════════════════════╝" << std::endl;
  
  return 0;
}

