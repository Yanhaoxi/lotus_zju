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
#include <cmath>

using namespace z3;

namespace SymAbs {

namespace {

expr bv_signed_to_int(const expr& bv) {
    context& ctx = bv.ctx();
    const unsigned w = bv.get_sort().bv_size();
    expr msb = z3_ext::extract(w - 1, w - 1, bv);
    expr unsigned_val = to_expr(ctx, Z3_mk_bv2int(ctx, bv, false));
    int64_t two_pow_w_val = 1LL << static_cast<int>(w);
    expr two_pow_w = ctx.int_val(two_pow_w_val);
    return ite(msb == ctx.bv_val(1, 1), unsigned_val - two_pow_w, unsigned_val);
}

int64_t div_floor(int64_t num, int64_t denom) {
    assert(denom > 0 && "denominator must be positive");
    if (num >= 0) {
        return num / denom;
    }
    return -static_cast<int64_t>((-num + denom - 1) / denom);
}

} // namespace

z3::expr oct_constraint_to_expr(const OctagonalConstraint& c) {
    context& ctx = c.var_i.ctx();
    expr lhs = ctx.int_val(0);
    lhs = lhs + ctx.int_val(c.lambda_i) * bv_signed_to_int(c.var_i);
    if (!c.unary) {
        lhs = lhs + ctx.int_val(c.lambda_j) * bv_signed_to_int(c.var_j);
    }
    return lhs <= ctx.int_val(c.bound);
}

std::vector<OctagonalConstraint> alpha_oct_V(
    z3::expr phi,
    const std::vector<z3::expr>& variables,
    const AbstractionConfig& config) {
    
    std::vector<OctagonalConstraint> constraints;
    
    if (variables.empty()) {
        return constraints;
    }
    
    context& ctx = phi.ctx();
    
    // Algorithm 8: Iterate over all pairs (v_i, v_j) and all (λ_i, λ_j) ∈ {-1, 1} × {-1, 1}
    for (size_t i = 0; i < variables.size(); ++i) {
        for (size_t j = i; j < variables.size(); ++j) {
            std::vector<std::pair<int, int>> coeff_pairs = {
                {1, 1},   // v_i + v_j ≤ d
                {1, -1},  // v_i - v_j ≤ d
                {-1, 1},  // -v_i + v_j ≤ d
                {-1, -1}  // -v_i - v_j ≤ d
            };
            
            for (const auto& pair : coeff_pairs) {
                int lambda_i = pair.first;
                int lambda_j = pair.second;
                LinearExpression lexpr;

                if (i == j) {
                    // Mixed signs cancel; only keep unary constraints (±2·v_i ≤ d)
                    if (lambda_i != lambda_j) {
                        continue;
                    }
                    lexpr.variables.push_back(variables[i]);
                    lexpr.coefficients.push_back(2 * lambda_i);
                } else {
                    lexpr.variables.push_back(variables[i]);
                    lexpr.coefficients.push_back(lambda_i);
                    lexpr.variables.push_back(variables[j]);
                    lexpr.coefficients.push_back(lambda_j);
                }

                auto bound = alpha_lin_exp(phi, lexpr, config);
                if (!bound.hasValue()) {
                    continue;
                }

                if (i == j) {
                    // Normalize 2·v_i ≤ d  ⇒  v_i ≤ ⌊d/2⌋
                    int64_t normalized = div_floor(bound.getValue(), 2);
                    constraints.emplace_back(variables[i], lambda_i, normalized);
                } else {
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
