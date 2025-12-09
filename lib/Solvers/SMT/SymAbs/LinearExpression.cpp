/**
 * @file LinearExpression.cpp
 * @brief Implementation of Algorithm 7: α_lin-exp
 * 
 * Computes the least upper bound of a linear expression Σ λ_i · ⟨⟨v_i⟩⟩
 * subject to a formula φ using bit-by-bit maximization.
 */

#include "Solvers/SMT/SymAbs/SymbolicAbstraction.h"
#include "Verification/SymbolicAbstraction/Utils/Z3APIExtension.h"
#include <z3++.h>
#include <z3.h>
#include <cassert>
#include <cmath>
#include <llvm/ADT/Optional.h>

using namespace z3;

namespace SymAbs {

/**
 * @brief Convert a signed bit-vector to an unbounded integer expression.
 *
 * We use an ITE to interpret the bit-vector in two's complement and avoid
 * wrap-around during arithmetic in the optimization phase.
 */
static expr bv_signed_to_int(const expr& bv) {
    context& ctx = bv.ctx();
    const unsigned w = bv.get_sort().bv_size();
    expr msb = z3_ext::extract(w - 1, w - 1, bv);
    expr unsigned_val = to_expr(ctx, Z3_mk_bv2int(ctx, bv, false));
    int64_t two_pow_w_val = 1LL << static_cast<int>(w);
    expr two_pow_w = ctx.int_val(two_pow_w_val);
    return ite(msb == ctx.bv_val(1, 1), unsigned_val - two_pow_w, unsigned_val);
}

/**
 * @brief Build an unbounded integer representation of Σ λ_i · ⟨⟨v_i⟩⟩.
 */
static expr build_integer_linear_expr(const LinearExpression& lexpr, context& ctx) {
    assert(!lexpr.variables.empty());
    assert(lexpr.variables.size() == lexpr.coefficients.size());
    
    expr sum = ctx.int_val(0);
    for (size_t i = 0; i < lexpr.variables.size(); ++i) {
        expr v_int = bv_signed_to_int(lexpr.variables[i]);
        int64_t coeff = lexpr.coefficients[i];
        if (coeff == 0) continue;
        sum = sum + ctx.int_val(coeff) * v_int;
    }
    return sum;
}

llvm::Optional<int64_t> alpha_lin_exp(
    z3::expr phi,
    const LinearExpression& lexpr,
    const AbstractionConfig& config) {
    
    if (lexpr.variables.empty()) {
        return llvm::None;
    }
    
    context& ctx = phi.ctx();

    // Build integer version of the linear expression to avoid wrap-around.
    expr int_expr = build_integer_linear_expr(lexpr, ctx);
    
    // Determine extended bit-width w' per Proposition 4.1
    // For simplicity, we use a conservative estimate: w' = max_bitwidth + log2(|coeffs|)
    unsigned max_bv_size = 0;
    int64_t sum_abs_coeffs = 0;
    for (size_t i = 0; i < lexpr.variables.size(); ++i) {
        unsigned bv_size = lexpr.variables[i].get_sort().bv_size();
        if (bv_size > max_bv_size) max_bv_size = bv_size;
        sum_abs_coeffs += std::abs(lexpr.coefficients[i]);
    }
    
    // Extended width: need enough bits to represent the sum without overflow
    // Conservative estimate: w' = w + ceil(log2(sum_abs_coeffs + 1))
    unsigned w_prime = max_bv_size;
    if (sum_abs_coeffs > 0) {
        unsigned log2_sum = 0;
        int64_t temp = sum_abs_coeffs;
        while (temp > 0) {
            temp >>= 1;
            log2_sum++;
        }
        w_prime = max_bv_size + log2_sum + 1;
    }
    // Cap at reasonable size
    if (w_prime > 128) w_prime = 128;
    
    // Algorithm 7: Bit-by-bit maximization
    // κ ← Σ λ_i · ⟨⟨v_i⟩⟩ = ⟨⟨d⟩⟩
    // ψ ← φ ∧ κ
    expr d_bv = ctx.bv_const("d", w_prime);
    expr d_int = bv_signed_to_int(d_bv);
    expr kappa = (int_expr == d_int);
    expr psi = phi && kappa;
    
    solver sol(ctx);
    params p(ctx);
    p.set("timeout", config.timeout_ms);
    sol.set(p);
    
    // Check sign bit first (lines 3-9 of Algorithm 7)
    sol.push();
    sol.add(psi);
    expr sign_bit = z3_ext::extract(w_prime - 1, w_prime - 1, d_bv);
    sol.add(sign_bit == ctx.bv_val(0, 1));
    
    int64_t d = 0;
    if (sol.check() == sat) {
        // Positive value
        d = 0;
        sol.pop();
        sol.push();
        sol.add(psi);
        sol.add(sign_bit == ctx.bv_val(0, 1));
    } else {
        // Negative value
        sol.pop();
        d = -(1LL << (w_prime - 1)); // -2^(w'-1)
        sol.push();
        sol.add(psi);
        sol.add(sign_bit == ctx.bv_val(1, 1));
    }
    
    // Iterate over bits w'-2, ..., 0 (lines 10-18)
    for (int bit_pos = static_cast<int>(w_prime) - 2; bit_pos >= 0; --bit_pos) {
        // Try setting bit to 1
        sol.push();
        expr bit = z3_ext::extract(bit_pos, bit_pos, d_bv);
        sol.add(bit == ctx.bv_val(1, 1));
        
        if (sol.check() == sat) {
            // Bit can be 1, increment d
            d += (1LL << bit_pos);
            sol.pop();
            sol.push();
            sol.add(bit == ctx.bv_val(1, 1));
        } else {
            // Bit must be 0
            sol.pop();
            sol.push();
            sol.add(bit == ctx.bv_val(0, 1));
        }
    }
    
    sol.pop();
    return d;
}

llvm::Optional<int64_t> minimum(
    z3::expr phi,
    z3::expr variable,
    const AbstractionConfig& config) {
    
    // Minimize v by maximizing -v
    LinearExpression neg_expr;
    neg_expr.variables.push_back(variable);
    neg_expr.coefficients.push_back(-1);
    
    auto max_neg = alpha_lin_exp(phi, neg_expr, config);
    if (!max_neg.hasValue()) {
        return llvm::None;
    }
    
    return -max_neg.getValue();
}

llvm::Optional<int64_t> maximum(
    z3::expr phi,
    z3::expr variable,
    const AbstractionConfig& config) {
    
    LinearExpression expr;
    expr.variables.push_back(variable);
    expr.coefficients.push_back(1);
    
    return alpha_lin_exp(phi, expr, config);
}

} // namespace SymAbs
