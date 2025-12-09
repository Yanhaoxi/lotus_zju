#pragma once

/**
 * @file Octagon.h
 * @brief Octagonal abstraction (Algorithm 8: α_oct^V)
 */

#include "Solvers/SMT/SymAbs/Config.h"
#include "Solvers/SMT/SymAbs/LinearExpression.h"
#include <z3++.h>
#include <vector>
#include <cstdint>

namespace SymAbs {

/**
 * @brief Represents an octagonal constraint: ±⟨v_i⟩ ± ⟨v_j⟩ ≤ d
 */
struct OctagonalConstraint {
    z3::expr var_i;
    z3::expr var_j;
    int lambda_i;  // ±1
    int lambda_j;  // ±1
    int64_t bound; // d
    
    OctagonalConstraint(z3::expr vi, z3::expr vj, int li, int lj, int64_t d)
        : var_i(vi), var_j(vj), lambda_i(li), lambda_j(lj), bound(d) {}
};

/**
 * @brief Compute octagonal abstraction α_oct^V(φ)
 * 
 * Algorithm 8: Computes the least octagon describing the set of bit-vectors V
 * subject to formula φ.
 * 
 * @param phi The formula
 * @param variables The set of bit-vector variables V = {v_1, ..., v_n}
 * @param config Configuration
 * @return Vector of octagonal constraints
 */
std::vector<OctagonalConstraint> alpha_oct_V(
    z3::expr phi,
    const std::vector<z3::expr>& variables,
    const AbstractionConfig& config = AbstractionConfig{});

} // namespace SymAbs
