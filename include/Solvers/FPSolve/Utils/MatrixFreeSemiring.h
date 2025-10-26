/**
 * Matrix-Free Semiring Evaluation Utilities
 */

#pragma once

#include "Solvers/FPSolve/Semirings/FreeSemiring.h"
#include "Solvers/FPSolve/DataStructs/Matrix.h"

namespace fpsolve {

/**
 * Evaluate a matrix of FreeSemiring elements using a valuation
 * 
 * This function evaluates each FreeSemiring element in the matrix
 * according to the given valuation map, producing a matrix over
 * the target semiring SR.
 * 
 * @tparam SR Target semiring type
 * @param matrix Matrix of FreeSemiring elements to evaluate
 * @param valuation Mapping from variables to SR values
 * @return Matrix of evaluated SR elements
 */
template <typename SR>
Matrix<SR> FreeSemiringMatrixEval(const Matrix<FreeSemiring> &matrix,
    const ValuationMap<SR> &valuation) {

  const std::vector<FreeSemiring> &elements = matrix.getElements();
  std::vector<SR> result;

  /* We have a single Evaluator that is used for all evaluations of the elements
   * of the original matrix.  This way if different elements refer to the same
   * FreeSemiring subexpression, we memoize the result and reuse it. */
  Evaluator<SR> evaluator{valuation};

  for(auto &elem : elements) {
    result.emplace_back(elem.Eval(evaluator));
  }

  return Matrix<SR>(matrix.getRows(), std::move(result));
}

/**
 * FIXME: Temporary wrapper for compatibility with the old implementation.
 * Evaluates a single FreeSemiring element using a valuation
 */
template <typename SR>
SR FreeSemiring_eval(FreeSemiring elem,
    ValuationMap<SR> *valuation) {
  return elem.Eval(*valuation);
}

/**
 * FIXME: Temporary wrapper for compatibility with the old implementation.
 * Evaluates a matrix of FreeSemiring elements using a valuation
 */
template <typename SR>
Matrix<SR> FreeSemiring_eval(Matrix<FreeSemiring> matrix,
    ValuationMap<SR> *valuation) {
  return FreeSemiringMatrixEval(matrix, *valuation);
}

} // namespace fpsolve

