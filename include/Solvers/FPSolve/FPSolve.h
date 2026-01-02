/**
 * FPSolve - Main Header
 * Fixed-Point Solver based on Newton's method for omega-continuous semirings
 * 
 * Original paper: "Newton's Method for ω-Continuous Semirings" (ICALP 2010)
 */

#ifndef FPSOLVE_H
#define FPSOLVE_H

// Core data structures
#include "Solvers/FPSolve/DataStructs/Var.h"
#include "Solvers/FPSolve/DataStructs/Equations.h"
#include "Solvers/FPSolve/DataStructs/Matrix.h"
#include "Solvers/FPSolve/DataStructs/VarDegreeMap.h"

// Semirings
#include "Solvers/FPSolve/Semirings/BoolSemiring.h"
#include "Solvers/FPSolve/Semirings/CommutativeRExp.h"
#include "Solvers/FPSolve/Semirings/FloatSemiring.h"
#include "Solvers/FPSolve/Semirings/FreeSemiring.h"
#include "Solvers/FPSolve/Semirings/MaxMinSemiring.h"
#include "Solvers/FPSolve/Semirings/Semiring.h"
#include "Solvers/FPSolve/Semirings/TropicalSemiring.h"
#include "Solvers/FPSolve/Semirings/ViterbiSemiring.h"

// Optional: LossyFiniteAutomaton (requires libfa)
#ifdef HAVE_LIBFA
#include "Solvers/FPSolve/Semirings/LossyFiniteAutomaton.h"
#endif

// Polynomials
#include "Solvers/FPSolve/Polynomials/CommutativeMonomial.h"
#include "Solvers/FPSolve/Polynomials/CommutativePolynomial.h"
#include "Solvers/FPSolve/Polynomials/NonCommutativePolynomial.h"

// Solvers
#include "Solvers/FPSolve/Solvers/KleeneSolver.h"
#include "Solvers/FPSolve/Solvers/NewtonGeneric.h"
#include "Solvers/FPSolve/Solvers/SolverUtils.h"

// Parser
#include "Solvers/FPSolve/Parser/Parser.h"

namespace fpsolve {

/**
 * Solver selection helper
 */
template<typename SR>
struct SolverFactory {
  enum class SolverType {
    KLEENE,         // Simple Kleene iteration
    NEWTON_CL,      // Newton with concrete linear solver
    NEWTON_CLDU     // Newton with LDU decomposition (default, most efficient)
  };

  static ValuationMap<SR> solve(
      const Equations<SR>& equations,
      int max_iterations = 100,
      SolverType type = SolverType::NEWTON_CLDU) {
    
    switch (type) {
      case SolverType::KLEENE: {
        KleeneSolver<SR> solver;
        return solver.solve_fixpoint(equations, max_iterations);
      }
      case SolverType::NEWTON_CL: {
        NewtonCL<SR> solver;
        return solver.solve_fixpoint(equations, max_iterations);
      }
      case SolverType::NEWTON_CLDU:
      default: {
        NewtonCLDU<SR> solver;
        return solver.solve_fixpoint(equations, max_iterations);
      }
    }
  }
};

} // namespace fpsolve

#endif // FPSOLVE_H

