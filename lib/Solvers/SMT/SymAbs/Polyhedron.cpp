/**
 * @file Polyhedron.cpp
 * @brief Implementation of Algorithm 9: α_conv^V
 * 
 * Computes a convex polyhedron over-approximating φ by iteratively finding
 * extremal points (vertices) and computing their convex hull.
 */

#include "Solvers/SMT/SymAbs/SymbolicAbstraction.h"
#include "Verification/SymbolicAbstraction/Utils/Z3APIExtension.h"
#include <z3++.h>
#include <z3.h>
#include <set>
#include <algorithm>

#include <cassert>
#include <vector>

using namespace z3;

namespace SymAbs {

/**
 * @brief Interpret bit-vector as signed integer.
 */
static expr bv_signed_to_int(const expr& bv) {
    context& ctx = bv.ctx();
    unsigned w = bv.get_sort().bv_size();
    expr msb = z3_ext::extract(w - 1, w - 1, bv);
    expr unsigned_val = to_expr(ctx, Z3_mk_bv2int(ctx, bv, false));
    int64_t two_pow_w_val = 1LL << static_cast<int>(w);
    expr two_pow_w = ctx.int_val(two_pow_w_val);
    return ite(msb == ctx.bv_val(1, 1), unsigned_val - two_pow_w, unsigned_val);
}

/**
 * @brief Build z3 expression for inequality Σ a_i·v_i ≤ b (signed).
 */
static expr build_inequality(
    const AffineEquality& ineq,
    const std::vector<expr>& variables,
    context& ctx) {
    
    assert(ineq.coefficients.size() == variables.size());
    expr lhs = ctx.int_val(0);
    for (size_t i = 0; i < variables.size(); ++i) {
        if (ineq.coefficients[i] == 0) continue;
        lhs = lhs + ctx.int_val(ineq.coefficients[i]) * bv_signed_to_int(variables[i]);
    }
    expr rhs = ctx.int_val(ineq.constant);
    return lhs <= rhs;
}

/**
 * @brief Negation of current polyhedron (∨ of violated inequalities).
 */
static expr build_not_poly(
    const std::vector<AffineEquality>& poly,
    const std::vector<expr>& variables,
    context& ctx) {
    
    if (poly.empty()) {
        return ctx.bool_val(true);
    }
    
    expr_vector violations(ctx);
    for (const auto& ineq : poly) {
        assert(ineq.coefficients.size() == variables.size());
        expr lhs = ctx.int_val(0);
        for (size_t i = 0; i < variables.size(); ++i) {
            if (ineq.coefficients[i] == 0) continue;
            lhs = lhs + ctx.int_val(ineq.coefficients[i]) * bv_signed_to_int(variables[i]);
        }
        expr rhs = ctx.int_val(ineq.constant);
        violations.push_back(lhs > rhs);
    }
    
    return mk_or(violations);
}

std::vector<AffineEquality> alpha_conv_V(
    z3::expr phi,
    const std::vector<z3::expr>& variables,
    const AbstractionConfig& config) {
    
    if (variables.empty()) {
        return {};
    }
    
    context& ctx = phi.ctx();
    size_t n = variables.size();
    
    // Initialize: c ← ⊥_conv (no inequalities collected yet)
    std::vector<AffineEquality> polyhedron;
    
    solver sol(ctx);
    params p(ctx);
    p.set("timeout", config.timeout_ms);
    sol.set(p);
    sol.add(phi);
    
    unsigned iteration = 0;
    
    // Main loop: while φ ∧ ¬c is satisfiable
    while (iteration < config.max_iterations) {
        expr not_c = build_not_poly(polyhedron, variables, ctx);
        
        sol.push();
        sol.add(phi);
        sol.add(not_c);
        
        if (sol.check() != sat) {
            sol.pop();
            break; // φ ⊨ c, we're done
        }
        
        // Algorithm 9: For each variable, find extremal values and extract full models
        // Collect points p for convex hull computation
        std::vector<std::vector<int64_t>> points;
        
        for (size_t v_idx = 0; v_idx < n; ++v_idx) {
            // Compute minimum and maximum for variable v
            auto min_val = minimum(phi && not_c, variables[v_idx], config);
            auto max_val = maximum(phi && not_c, variables[v_idx], config);

            if (min_val.hasValue()) {
                // Find model m_ℓ where ⟨⟨v⟩⟩ = ℓ
                solver min_sol(ctx);
                min_sol.add(phi && not_c);
                unsigned bv_size = variables[v_idx].get_sort().bv_size();
                min_sol.add(variables[v_idx] == ctx.bv_val(static_cast<uint64_t>(min_val.getValue()), bv_size));
                
                if (min_sol.check() == sat) {
                    model m_ell = min_sol.get_model();
                    std::vector<int64_t> point;
                    for (const auto& var : variables) {
                        expr val = m_ell.eval(var, true);
                        if (val.is_numeral()) {
                            std::string num_str = Z3_get_numeral_string(ctx, val);
                            point.push_back(std::stoll(num_str));
                        } else if (val.is_bv()) {
                            unsigned bv_sz = val.get_sort().bv_size();
                            if (bv_sz <= 64) {
                                uint64_t unsigned_val = 0;
                                Z3_get_numeral_uint64(ctx, val, &unsigned_val);
                                int64_t signed_val = static_cast<int64_t>(unsigned_val);
                                if (bv_sz < 64) {
                                    int64_t mask = (1LL << bv_sz) - 1;
                                    int64_t sign_mask = 1LL << (bv_sz - 1);
                                    if (signed_val & sign_mask) {
                                        signed_val = signed_val | ~mask;
                                    }
                                }
                                point.push_back(signed_val);
                            } else {
                                point.push_back(0);
                            }
                        } else {
                            point.push_back(0);
                        }
                    }
                    points.push_back(point);
                }
            }

            if (max_val.hasValue()) {
                // Find model m_u where ⟨⟨v⟩⟩ = u
                solver max_sol(ctx);
                max_sol.add(phi && not_c);
                unsigned bv_size = variables[v_idx].get_sort().bv_size();
                max_sol.add(variables[v_idx] == ctx.bv_val(static_cast<uint64_t>(max_val.getValue()), bv_size));
                
                if (max_sol.check() == sat) {
                    model m_u = max_sol.get_model();
                    std::vector<int64_t> point;
                    for (const auto& var : variables) {
                        expr val = m_u.eval(var, true);
                        if (val.is_numeral()) {
                            std::string num_str = Z3_get_numeral_string(ctx, val);
                            point.push_back(std::stoll(num_str));
                        } else if (val.is_bv()) {
                            unsigned bv_sz = val.get_sort().bv_size();
                            if (bv_sz <= 64) {
                                uint64_t unsigned_val = 0;
                                Z3_get_numeral_uint64(ctx, val, &unsigned_val);
                                int64_t signed_val = static_cast<int64_t>(unsigned_val);
                                if (bv_sz < 64) {
                                    int64_t mask = (1LL << bv_sz) - 1;
                                    int64_t sign_mask = 1LL << (bv_sz - 1);
                                    if (signed_val & sign_mask) {
                                        signed_val = signed_val | ~mask;
                                    }
                                }
                                point.push_back(signed_val);
                            } else {
                                point.push_back(0);
                            }
                        } else {
                            point.push_back(0);
                        }
                    }
                    points.push_back(point);
                }
            }
        }
        
        // Add inequalities from the convex hull of collected points
        // For now, we add interval bounds for each variable based on the points
        // A full implementation would compute the actual convex hull
        if (!points.empty()) {
            for (size_t v_idx = 0; v_idx < n; ++v_idx) {
                int64_t min_val = points[0][v_idx];
                int64_t max_val = points[0][v_idx];
                for (const auto& point : points) {
                    if (point[v_idx] < min_val) min_val = point[v_idx];
                    if (point[v_idx] > max_val) max_val = point[v_idx];
                }
                
                AffineEquality geq;
                geq.variables = variables;
                geq.coefficients.assign(n, 0);
                geq.coefficients[v_idx] = -1;
                geq.constant = -min_val;
                polyhedron.push_back(geq);
                
                AffineEquality leq;
                leq.variables = variables;
                leq.coefficients.assign(n, 0);
                leq.coefficients[v_idx] = 1;
                leq.constant = max_val;
                polyhedron.push_back(leq);
            }
        }
        
        if (polyhedron.size() >= config.max_inequalities) {
            break;
        }

        sol.pop();
        ++iteration;

        if (polyhedron.size() >= config.max_inequalities) {
            break;
        }
    }

    return polyhedron;
}

std::vector<AffineEquality> relax_conv(
    z3::expr phi,
    const std::vector<AffineEquality>& template_constraints,
    const AbstractionConfig& config) {
    
    if (template_constraints.empty()) {
        return {};
    }
    
    context& ctx = phi.ctx();
    std::vector<AffineEquality> result;
    
    // For each template constraint, maximize the constant d
    for (const auto& template_eq : template_constraints) {
        LinearExpression lexpr;
        lexpr.variables = template_eq.variables;
        lexpr.coefficients = template_eq.coefficients;

        auto max_bound = alpha_lin_exp(phi, lexpr, config);
        if (!max_bound.hasValue()) {
            continue;
        }

        AffineEquality relaxed = template_eq;
        relaxed.constant = max_bound.getValue();
        result.push_back(relaxed);
    }

    return result;
}

} // namespace SymAbs
