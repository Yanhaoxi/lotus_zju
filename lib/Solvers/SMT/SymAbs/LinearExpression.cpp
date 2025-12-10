/**
 * @file LinearExpression.cpp
 * @brief Implementation of Algorithm 7: α_lin-exp
 *
 * Computes the least upper bound of a linear expression Σ λ_i · ⟨⟨v_i⟩⟩
 * subject to a formula φ.  We work over unbounded integers obtained via bv2int
 * to avoid wrap-around and use Z3's optimize engine to obtain an exact optimum
 * (falling back to a bounded linear search if the solver returns unknown).
 */

#include "Solvers/SMT/SymAbs/LinearExpression.h"
#include "Solvers/SMT/SymAbs/SymAbsUtils.h"
#include "Verification/SymbolicAbstraction/Utils/Z3APIExtension.h"
#include <z3++.h>
#include <z3.h>
#include <cassert>
#include <llvm/ADT/Optional.h>

using namespace z3;

namespace SymAbs {

/**
 * @brief Build an unbounded integer representation of Σ λ_i · ⟨⟨v_i⟩⟩.
 */
static expr build_integer_linear_expr(const LinearExpression &lexpr, context &ctx) {
    assert(!lexpr.variables.empty());
    assert(lexpr.variables.size() == lexpr.coefficients.size());

    expr sum = ctx.int_val(0);
    for (size_t i = 0; i < lexpr.variables.size(); ++i) {
        expr v_int = SymAbs::bv_signed_to_int(lexpr.variables[i]);
        int64_t coeff = lexpr.coefficients[i];
        if (coeff == 0)
            continue;
        sum = sum + ctx.int_val(coeff) * v_int;
    }
    return sum;
}

llvm::Optional<int64_t> alpha_lin_exp(const z3::expr &phi, const LinearExpression &lexpr,
                                      const AbstractionConfig &config) {
    if (lexpr.variables.empty()) {
        return llvm::None;
    }

    context &ctx = phi.ctx();
    expr int_expr = build_integer_linear_expr(lexpr, ctx);

    // First attempt: use Z3 optimize to obtain a model with maximal value.
    optimize opt(ctx);
    params p(ctx);
    p.set("timeout", config.timeout_ms);
    opt.set(p);
    opt.add(phi);
    opt.maximize(int_expr);

    auto check_res = opt.check();
    if (check_res == sat) {
        model m = opt.get_model();
        expr val = m.eval(int_expr, true);
        auto as_int = SymAbs::to_int64(val);
        if (as_int.hasValue()) {
            return as_int;
        }
    }

    if (check_res == unsat) {
        return llvm::None;
    }

    // Fallback: bounded ascending search using a plain solver when optimize
    // returns unknown or non-numeral values.
    solver sol(ctx);
    sol.set(p);
    sol.add(phi);

    if (sol.check() != sat) {
        return llvm::None;
    }

    model m0 = sol.get_model();
    expr v0 = m0.eval(int_expr, true);
    auto best_opt = SymAbs::to_int64(v0);
    if (!best_opt.hasValue()) {
        return llvm::None;
    }

    int64_t best = best_opt.getValue();
    unsigned iter = 0;

    while (iter < config.max_iterations) {
        sol.push();
        sol.add(int_expr > ctx.int_val(best));
        auto res = sol.check();
        if (res != sat) {
            sol.pop();
            break;
        }
        model m_new = sol.get_model();
        expr v_new = m_new.eval(int_expr, true);
        auto v_int = SymAbs::to_int64(v_new);
        sol.pop();
        if (!v_int.hasValue()) {
            break;
        }
        best = v_int.getValue();
        ++iter;
    }

    return best_opt.hasValue() ? llvm::Optional<int64_t>(best) : llvm::None;
}

llvm::Optional<int64_t> minimum(const z3::expr &phi, const z3::expr &variable,
                                const AbstractionConfig &config) {
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

llvm::Optional<int64_t> maximum(const z3::expr &phi, const z3::expr &variable,
                                const AbstractionConfig &config) {
    LinearExpression expr;
    expr.variables.push_back(variable);
    expr.coefficients.push_back(1);

    return alpha_lin_exp(phi, expr, config);
}

} // namespace SymAbs
