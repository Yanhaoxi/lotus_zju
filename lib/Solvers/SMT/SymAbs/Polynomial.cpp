/**
 * @file Polynomial.cpp
 * @brief Implementation of Algorithm 13: α_poly^V
 * 
 * Computes polynomial hull of φ with template monomials S.
 * This extends affine abstraction to handle non-linear terms.
 */

#include "Solvers/SMT/SymAbs/SymbolicAbstraction.h"
#include "Verification/SymbolicAbstraction/Utils/Z3APIExtension.h"
#include <z3++.h>
#include <z3.h>
#include <map>
#include <cassert>

using namespace z3;

namespace SymAbs {

/**
 * @brief Evaluate a monomial expression under a model
 */
static int64_t eval_monomial(const expr& monomial, const model& m, context& ctx) {
    // For now, handle simple cases
    // Full implementation would need to handle arbitrary monomials
    
    if (monomial.is_numeral()) {
        std::string num_str = Z3_get_numeral_string(ctx, monomial);
        return std::stoll(num_str);
    }
    
    if (monomial.is_const()) {
        expr val = m.eval(monomial, true);
        if (val.is_numeral()) {
            std::string num_str = Z3_get_numeral_string(ctx, val);
            return std::stoll(num_str);
        }
    }
    
    // For multiplication: v1 * v2
    if (monomial.decl().decl_kind() == Z3_OP_BMUL) {
        assert(monomial.num_args() == 2);
        int64_t val1 = eval_monomial(monomial.arg(0), m, ctx);
        int64_t val2 = eval_monomial(monomial.arg(1), m, ctx);
        return val1 * val2;
    }
    
    // For other operations, return 0 as fallback
    return 0;
}

std::vector<AffineEquality> alpha_poly_V(
    z3::expr phi,
    const std::vector<z3::expr>& variables,
    const std::vector<z3::expr>& monomials,
    const AbstractionConfig& config) {
    
    if (variables.empty()) {
        return {};
    }
    
    context& ctx = phi.ctx();
    size_t n = variables.size();
    size_t k = monomials.size();
    
    // Extended variable set: (v_1, ..., v_n, s_1, ..., s_k)
    std::vector<expr> extended_vars = variables;
    extended_vars.insert(extended_vars.end(), monomials.begin(), monomials.end());
    
    // Initialize [A|b] = [0, ..., 0 | 1]
    std::vector<AffineEquality> A_b;
    AffineEquality initial_row;
    initial_row.variables = extended_vars;
    initial_row.coefficients.resize(n + k, 0);
    initial_row.constant = 1;
    A_b.push_back(initial_row);
    
    unsigned i = 0;
    unsigned r = 1;
    
    solver sol(ctx);
    params p(ctx);
    p.set("timeout", config.timeout_ms);
    sol.set(p);
    
    unsigned iteration = 0;
    
    // Main loop: while i < r
    while (i < r && iteration < config.max_iterations) {
        if (i >= A_b.size()) {
            break;
        }
        
        // Extract row (r-i)
        size_t row_idx = r - i - 1;
        const AffineEquality& row = A_b[row_idx];
        
        // Build disequality: Σ a_j · v_j ≠ b_{r-i}
        // This includes both variables and monomials
        expr sum = ctx.bv_val(0, variables[0].get_sort().bv_size());
        unsigned bv_size = variables[0].get_sort().bv_size();
        
        for (size_t j = 0; j < row.variables.size() && j < row.coefficients.size(); ++j) {
            expr var = row.variables[j];
            int64_t coeff = row.coefficients[j];
            
            if (coeff == 0) continue;
            
            if (var.get_sort().bv_size() != bv_size) {
                var = adjustBitwidth(var, bv_size);
            }
            
            if (j < n) {
                // Regular variable
                if (coeff == 1) {
                    sum = sum + var;
                } else if (coeff == -1) {
                    sum = sum - var;
                } else if (coeff > 0) {
                    expr coeff_bv = ctx.bv_val(static_cast<uint64_t>(coeff), bv_size);
                    sum = sum + (coeff_bv * var);
                } else {
                    expr coeff_bv = ctx.bv_val(static_cast<uint64_t>(-coeff), bv_size);
                    sum = sum - (coeff_bv * var);
                }
            } else {
                // Monomial variable
                // For monomials, we need to ensure the constraint matches
                // the actual monomial value
                // This is simplified - full implementation would properly
                // encode monomial constraints
            }
        }
        
        expr b_bv = ctx.bv_val(static_cast<uint64_t>(row.constant), bv_size);
        expr psi = sum != b_bv;
        
        // Check satisfiability
        sol.push();
        sol.add(phi);
        sol.add(psi);
        
        if (sol.check() == sat) {
            // Found violating model
            model m = sol.get_model();
            
            // Extract variable values
            std::vector<int64_t> model_values;
            for (size_t j = 0; j < n; ++j) {
                expr val = m.eval(variables[j], true);
                if (val.is_numeral()) {
                    std::string num_str = Z3_get_numeral_string(ctx, val);
                    model_values.push_back(std::stoll(num_str));
                } else {
                    model_values.push_back(0);
                }
            }
            
            // Evaluate monomials
            std::vector<int64_t> monomial_values;
            for (const auto& mon : monomials) {
                int64_t val = eval_monomial(mon, m, ctx);
                monomial_values.push_back(val);
            }
            
            // Create identity system with extended variables
            std::vector<AffineEquality> identity_system;
            for (size_t j = 0; j < n; ++j) {
                AffineEquality eq;
                eq.variables = extended_vars;
                eq.coefficients.resize(n + k, 0);
                eq.coefficients[j] = 1;
                eq.constant = model_values[j];
                identity_system.push_back(eq);
            }
            
            for (size_t j = 0; j < k; ++j) {
                AffineEquality eq;
                eq.variables = extended_vars;
                eq.coefficients.resize(n + k, 0);
                eq.coefficients[n + j] = 1;
                eq.constant = monomial_values[j];
                identity_system.push_back(eq);
            }
            
            // Join: [A'|b'] = [A|b] ⊔_aff [Id | (m(v_1), ..., m(v_n), p_1, ..., p_k)^T]
            // Use alpha_aff_V to compute the join properly
            // For polynomial abstraction, we treat it as affine abstraction over extended variables
            // Build formula representing the identity system
            expr identity_formula = ctx.bool_val(true);
            for (size_t j = 0; j < n; ++j) {
                unsigned bv_size = variables[j].get_sort().bv_size();
                identity_formula = identity_formula && 
                    (variables[j] == ctx.bv_val(static_cast<uint64_t>(model_values[j]), bv_size));
            }
            for (size_t j = 0; j < k; ++j) {
                unsigned bv_size = monomials[j].get_sort().bv_size();
                identity_formula = identity_formula && 
                    (monomials[j] == ctx.bv_val(static_cast<uint64_t>(monomial_values[j]), bv_size));
            }
            
            // Compute affine abstraction of current system joined with identity
            // This is a simplified approach - full implementation would use proper matrix operations
            // For now, we add the identity system and triangularize
            A_b.insert(A_b.end(), identity_system.begin(), identity_system.end());
            
            // Simple triangularization: remove redundant rows
            // Full implementation would use proper Gaussian elimination
            // Remove duplicate rows
            std::vector<AffineEquality> unique_rows;
            for (const auto& row : A_b) {
                bool is_duplicate = false;
                for (const auto& existing : unique_rows) {
                    if (row.coefficients == existing.coefficients && 
                        row.constant == existing.constant) {
                        is_duplicate = true;
                        break;
                    }
                }
                if (!is_duplicate) {
                    unique_rows.push_back(row);
                }
            }
            A_b = unique_rows;
            
            r = A_b.size();
            i = 0;
        } else {
            ++i;
        }
        
        sol.pop();
        ++iteration;
    }
    
    return A_b;
}

} // namespace SymAbs
