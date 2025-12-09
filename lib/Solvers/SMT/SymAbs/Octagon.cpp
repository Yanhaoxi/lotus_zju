/**
 * @file Octagon.cpp
 * @brief Implementation of Algorithm 8: α_oct^V
 * 
 * Computes the least octagon describing a set of bit-vectors V subject to formula φ.
 * An octagon is a conjunction of constraints of the form: ±⟨v_i⟩ ± ⟨v_j⟩ ≤ d
 */

#include "Solvers/SMT/SymAbs/SymbolicAbstraction.h"
#include "Verification/SymbolicAbstraction/Utils/Z3APIExtension.h"
#include <z3++.h>
#include <z3.h>
#include <algorithm>
#include <cassert>
#include <llvm/ADT/Optional.h>

using namespace z3;

namespace SymAbs {

std::vector<OctagonalConstraint> alpha_oct_V(
    z3::expr phi,
    const std::vector<z3::expr>& variables,
    const AbstractionConfig& config) {
    
    std::vector<OctagonalConstraint> constraints;
    
    if (variables.empty()) {
        return constraints;
    }
    
    context& ctx = phi.ctx();
    
    // Algorithm 8: Iterate over all pairs (v_i, v_j) and all coefficient combinations
    // According to the paper, we iterate over all pairs and all (λ_i, λ_j) ∈ {-1, 1} × {-1, 1}
    for (size_t i = 0; i < variables.size(); ++i) {
        for (size_t j = i; j < variables.size(); ++j) {
            // For each pair, consider all combinations of λ_i, λ_j ∈ {-1, 1}
            std::vector<std::pair<int, int>> coeff_pairs = {
                {1, 1},   // v_i + v_j ≤ d
                {1, -1},  // v_i - v_j ≤ d
                {-1, 1},  // -v_i + v_j ≤ d
                {-1, -1}  // -v_i - v_j ≤ d
            };
            
            for (const auto& [lambda_i, lambda_j] : coeff_pairs) {
                // Build linear expression: λ_i · v_i + λ_j · v_j
                LinearExpression lexpr;
                
                if (i == j) {
                    // Single variable case: represent as 2·⟨v_i⟩ ≤ d or -2·⟨v_i⟩ ≤ d
                    // per the paper's note: "⟨v_i⟩ ≤ d is represented as 2·⟨v_i⟩ ≤ 2·d"
                    lexpr.variables.push_back(variables[i]);
                    lexpr.coefficients.push_back(lambda_i + lambda_j); // 2 or -2
                } else {
                    // Two variable case
                    lexpr.variables.push_back(variables[i]);
                    lexpr.coefficients.push_back(lambda_i);
                    lexpr.variables.push_back(variables[j]);
                    lexpr.coefficients.push_back(lambda_j);
                }
                
                // Compute least upper bound using α_lin-exp
                auto bound = alpha_lin_exp(phi, lexpr, config);
                
                if (bound.hasValue()) {
                    constraints.emplace_back(
                        variables[i],
                        variables[j],
                        lambda_i,
                        lambda_j,
                        bound.getValue()
                    );
                }
            }
        }
    }
    
    return constraints;
}

} // namespace SymAbs
