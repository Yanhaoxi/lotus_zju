/**
 * Kleene Solver for FPSolve
 */

#ifndef FPSOLVE_KLEENE_SOLVER_H
#define FPSOLVE_KLEENE_SOLVER_H

#include "Solvers/FPSolve/DataStructs/Var.h"
#include "Solvers/FPSolve/DataStructs/Equations.h"
#include "Solvers/FPSolve/Polynomials/CommutativePolynomial.h"
#include <vector>

namespace fpsolve {

template <typename SR>
class KleeneSolver {
public:
  ValuationMap<SR> solve_fixpoint(
      const Equations<SR>& equations,
      int max_iter) {
    
    std::vector<VarId> poly_vars;
    std::vector<CommutativePolynomial<SR>> F;
    for (const auto &eq : equations) {
      poly_vars.push_back(eq.first);
      F.push_back(eq.second);
    }

    // Initialize all variables to null
    ValuationMap<SR> values;
    for (const auto& var : poly_vars) {
      values[var] = SR::null();
    }

    // Iterate until convergence or max iterations
    for (int iter = 0; iter < max_iter; ++iter) {
      ValuationMap<SR> new_values;
      bool changed = false;

      for (std::size_t i = 0; i < F.size(); ++i) {
        SR new_val = F[i].eval(values);
        new_values[poly_vars[i]] = new_val;
        
        if (!SR::IsIdempotent()) {
          if (!(new_val == values[poly_vars[i]])) {
            changed = true;
          }
        } else {
          if (!(new_val == values[poly_vars[i]])) {
            changed = true;
          }
        }
      }

      values = std::move(new_values);

      if (!changed) {
        break;
      }
    }

    return values;
  }
};

} // namespace fpsolve

#endif // FPSOLVE_KLEENE_SOLVER_H

