/**
 * Commutative Polynomial implementation
 */

#include "Solvers/FPSolve/Polynomials/CommutativePolynomial.h"
#include "Solvers/FPSolve/Semirings/BoolSemiring.h"
#include "Solvers/FPSolve/Semirings/FloatSemiring.h"
#include "Solvers/FPSolve/Semirings/TropicalSemiring.h"
#include "Solvers/FPSolve/Semirings/FreeSemiring.h"

namespace fpsolve {

template <typename SR>
Matrix<CommutativePolynomial<SR>> CommutativePolynomial<SR>::jacobian(
    const std::vector<CommutativePolynomial<SR>>& polynomials,
    const std::vector<VarId>& variables) {
  
  std::size_t n = polynomials.size();
  std::size_t m = variables.size();
  
  std::vector<CommutativePolynomial<SR>> jacobian_elements;
  jacobian_elements.reserve(n * m);

  for (const auto& poly : polynomials) {
    for (const auto& var : variables) {
      // Compute partial derivative with respect to var
      CommutativePolynomial<SR> derivative;
      
      for (const auto& term : poly.monomials_) {
        const CommutativeMonomial& monomial = term.first;
        const SR& coeff = term.second;
        
        Degree var_degree = monomial.variables_.GetDegree(var);
        if (var_degree > 0) {
          // Create new monomial with degree reduced by 1
          std::map<VarId, Degree> new_vars;
          for (const auto& vd : monomial.variables_) {
            if (vd.first == var) {
              if (vd.second > 1) {
                new_vars[vd.first] = vd.second - 1;
              }
            } else {
              new_vars[vd.first] = vd.second;
            }
          }
          
          CommutativeMonomial new_monomial(new_vars);
          SR new_coeff = coeff;
          for (Degree i = 0; i < var_degree; ++i) {
            new_coeff *= SR::one(); // Multiply by degree (simplified)
          }
          
          derivative.InsertMonomial(new_monomial, new_coeff);
          derivative.variables_.Merge(new_monomial.variables_);
        }
      }
      
      if (derivative.monomials_.empty()) {
        derivative.monomials_[CommutativeMonomial()] = SR::null();
      }
      
      jacobian_elements.push_back(derivative);
    }
  }

  return Matrix<CommutativePolynomial<SR>>(n, std::move(jacobian_elements));
}

// Explicit template instantiations for common types
template class CommutativePolynomial<BoolSemiring>;
template class CommutativePolynomial<FloatSemiring>;
template class CommutativePolynomial<TropicalSemiring>;
template class CommutativePolynomial<FreeSemiring>;

} // namespace fpsolve

