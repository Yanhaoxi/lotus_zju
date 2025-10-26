/**
 * Generic Newton Solver for FPSolve
 */

#ifndef FPSOLVE_NEWTON_GENERIC_H
#define FPSOLVE_NEWTON_GENERIC_H

#include "Solvers/FPSolve/DataStructs/Var.h"
#include "Solvers/FPSolve/DataStructs/Matrix.h"
#include "Solvers/FPSolve/DataStructs/Equations.h"
#include "Solvers/FPSolve/Polynomials/CommutativePolynomial.h"
#include <vector>
#include <algorithm>

namespace fpsolve {

// Linear equation solver parametrized by semiring
template <typename SR>
class LinEqSolver {
public:
  virtual ~LinEqSolver() = default;
  virtual Matrix<SR> solve_lin_at(const Matrix<SR>& values, 
                                   const Matrix<SR>& rhs,
                                   const std::vector<VarId>& variables) = 0;
};

// Delta generator parametrized by semiring
template <typename SR>
class DeltaGenerator {
public:
  virtual ~DeltaGenerator() = default;
  virtual Matrix<SR> delta_at(const Matrix<SR>& newton_update,
                               const Matrix<SR>& previous_newton_values) = 0;
};

// Concrete Linear Solver using LDU decomposition
template <typename SR>
class CommutativeConcreteLinSolver : public LinEqSolver<SR> {
private:
  Matrix<CommutativePolynomial<SR>> jacobian_;
  ValuationMap<SR> valuation_;

public:
  CommutativeConcreteLinSolver(
      const std::vector<CommutativePolynomial<SR>>& F,
      const std::vector<VarId>& variables)
    : jacobian_(CommutativePolynomial<SR>::jacobian(F, variables)) {}

  Matrix<SR> solve_lin_at(const Matrix<SR>& values, 
                         const Matrix<SR>& rhs,
                         const std::vector<VarId>& variables) override {
    assert(values.getColumns() == 1);
    assert(variables.size() == values.getRows());

    for (std::size_t i = 0; i < variables.size(); ++i) {
      valuation_[variables[i]] = values.At(i, 0);
    }

    std::vector<SR> result_vec;
    for (const auto &poly : jacobian_.getElements()) {
      result_vec.emplace_back(poly.eval(valuation_));
    }

    return Matrix<SR>{jacobian_.getRows(), std::move(result_vec)}.star() * rhs;
  }
};

// LDU-based Linear Solver (most efficient)
template <typename SR>
class LinSolver_CLDU : public LinEqSolver<SR> {
private:
  Matrix<CommutativePolynomial<SR>> jacobian_;
  ValuationMap<SR> valuation_;

public:
  LinSolver_CLDU(
      const std::vector<CommutativePolynomial<SR>>& F,
      const std::vector<VarId>& variables)
    : jacobian_(CommutativePolynomial<SR>::jacobian(F, variables)) {}

  Matrix<SR> solve_lin_at(const Matrix<SR>& values,
                         const Matrix<SR>& rhs,
                         const std::vector<VarId>& variables) override {
    assert(values.getColumns() == 1);
    assert(variables.size() == values.getRows());

    for (std::size_t i = 0; i < variables.size(); ++i) {
      valuation_[variables[i]] = values.At(i, 0);
    }

    std::vector<SR> result_vec;
    for (const auto &poly : jacobian_.getElements()) {
      result_vec.emplace_back(poly.eval(valuation_));
    }

    return Matrix<SR>{jacobian_.getRows(), std::move(result_vec)}.solve_LDU(rhs);
  }
};

// Commutative Delta Generator
template <typename SR>
class CommutativeDeltaGenerator : public DeltaGenerator<SR> {
private:
  const std::vector<CommutativePolynomial<SR>>& polynomials_;
  const std::vector<VarId>& poly_vars_;
  ValuationMap<SR> current_valuation_;
  ValuationMap<SR> zero_valuation_;

public:
  CommutativeDeltaGenerator(
      const std::vector<CommutativePolynomial<SR>>& ps,
      const std::vector<VarId>& pvs)
      : polynomials_(ps), poly_vars_(pvs) {
    for (const auto& var : poly_vars_) {
      zero_valuation_[var] = SR::null();
    }
  }

  Matrix<SR> delta_at(const Matrix<SR>& newton_update,
                     const Matrix<SR>& previous_newton_values) override {
    assert(previous_newton_values.getColumns() == 1 && newton_update.getColumns() == 1);
    auto num_variables = poly_vars_.size();
    assert(num_variables == previous_newton_values.getRows() &&
           num_variables == newton_update.getRows());

    std::vector<SR> delta_vector;
    ValuationMap<SR> newton_update_map;

    for (std::size_t i = 0; i < num_variables; ++i) {
      current_valuation_[poly_vars_[i]] = previous_newton_values.At(i, 0);
      newton_update_map[poly_vars_[i]] = newton_update.At(i, 0);
    }

    for (const auto& polynomial : polynomials_) {
      SR delta = SR::null();
      Degree polynomial_max_degree = polynomial.get_degree();

      if (polynomial_max_degree <= 1) {
        delta = SR::null();
      } else {
        // Simplified: for quadratic case
        delta = SR::null(); // Would compute higher-order derivatives here
      }

      delta_vector.emplace_back(std::move(delta));
    }

    return Matrix<SR>(delta_vector.size(), std::move(delta_vector));
  }
};

// Generic Newton Solver
template <typename SR,
          template <typename> class LinEqSolverTemplate,
          template <typename> class DeltaGeneratorTemplate>
class GenericNewton {
  using LinSolver = LinEqSolverTemplate<SR>;
  using DeltaGen = DeltaGeneratorTemplate<SR>;

public:
  ValuationMap<SR> solve_fixpoint(
      const Equations<SR>& equations,
      int max_iter) {
    
    std::vector<CommutativePolynomial<SR>> F;
    std::vector<VarId> poly_vars;
    for (const auto &eq : equations) {
      poly_vars.push_back(eq.first);
      F.push_back(eq.second);
    }
    
    Matrix<SR> result = solve_fixpoint(F, poly_vars, max_iter);

    ValuationMap<SR> solution;
    auto result_vec = result.getElements();
    assert(result_vec.size() == poly_vars.size());
    for (std::size_t i = 0; i < result_vec.size(); ++i) {
      solution.insert(std::make_pair(poly_vars[i], result_vec[i]));
    }
    return solution;
  }

  Matrix<SR> solve_fixpoint(
      const std::vector<CommutativePolynomial<SR>>& polynomials,
      const std::vector<VarId>& variables,
      std::size_t max_iter) {

    assert(polynomials.size() == variables.size());

    Matrix<CommutativePolynomial<SR>> F_mat{polynomials.size(), polynomials};
    Matrix<SR> newton_values{polynomials.size(), 1};
    Matrix<SR> previous_newton_values{polynomials.size(), 1};

    ValuationMap<SR> values;
    for (const auto& variable : variables) {
      values.insert(std::make_pair(variable, SR::null()));
    }
    
    // Initial delta: F(0)
    std::vector<SR> delta_vec;
    for (const auto& poly : polynomials) {
      delta_vec.push_back(poly.eval(values));
    }
    Matrix<SR> delta{polynomials.size(), std::move(delta_vec)};
    Matrix<SR> newton_update{polynomials.size(), 1};

    LinSolver lin_solver(polynomials, variables);
    DeltaGen delta_gen(polynomials, variables);

    for (unsigned int i = 0; i < max_iter; ++i) {
      newton_update = lin_solver.solve_lin_at(newton_values, delta, variables);

      if (!SR::IsIdempotent() && i + 1 < max_iter) {
        delta = delta_gen.delta_at(newton_update, newton_values);
      }

      if (!SR::IsIdempotent()) {
        newton_values = newton_values + newton_update;
      } else {
        newton_values = newton_update;
      }
    }

    return newton_values;
  }
};

// Standard Newton solver variants
template <typename SR>
using NewtonCL = GenericNewton<SR, CommutativeConcreteLinSolver, CommutativeDeltaGenerator>;

template <typename SR>
using NewtonCLDU = GenericNewton<SR, LinSolver_CLDU, CommutativeDeltaGenerator>;

} // namespace fpsolve

#endif // FPSOLVE_NEWTON_GENERIC_H

