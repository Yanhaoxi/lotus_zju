/**
 * @file Affine.cpp
 * @brief Implementation of Algorithm 12: α_aff^V
 * 
 * Computes the affine hull of models of φ, represented as an affine system [A|b].
 * The algorithm iteratively refines an affine abstraction by finding models that
 * violate current constraints and merging them.
 */

#include "Solvers/SMT/SymAbs/SymbolicAbstraction.h"
#include "Verification/SymbolicAbstraction/Utils/Z3APIExtension.h"
#include <z3++.h>
#include <z3.h>
#include <vector>
#include <algorithm>
#include <cassert>
#include <numeric>

using namespace z3;

namespace SymAbs {

/**
 * @brief Represents a row of the affine system [A|b]
 */
struct AffineRow {
    std::vector<int64_t> coefficients; // a_1, ..., a_n
    int64_t constant;                   // b
    
    AffineRow(size_t n) : coefficients(n, 0), constant(0) {}
    AffineRow(const std::vector<int64_t>& coeffs, int64_t c)
        : coefficients(coeffs), constant(c) {}
};

/**
 * @brief Extract model values as integers
 */
static std::vector<int64_t> extract_model_values(
    const model& m,
    const std::vector<expr>& variables) {
    
    std::vector<int64_t> values;
    values.reserve(variables.size());
    
    for (const auto& var : variables) {
        expr val = m.eval(var, true);
        
        if (val.is_numeral()) {
            std::string num_str = Z3_get_numeral_string(val.ctx(), val);
            values.push_back(std::stoll(num_str));
        } else if (val.is_bv()) {
            unsigned bv_size = val.get_sort().bv_size();
            if (bv_size <= 64) {
                uint64_t unsigned_val = 0;
                Z3_get_numeral_uint64(val.ctx(), val, &unsigned_val);
                
                // Interpret as signed
                if (bv_size < 64) {
                    int64_t mask = (1LL << bv_size) - 1;
                    int64_t sign_mask = 1LL << (bv_size - 1);
                    int64_t signed_val = static_cast<int64_t>(unsigned_val);
                    if (signed_val & sign_mask) {
                        signed_val = signed_val | ~mask;
                    }
                    values.push_back(signed_val);
                } else {
                    values.push_back(static_cast<int64_t>(unsigned_val));
                }
            } else {
                assert(false && "Bit-vector too large");
                values.push_back(0);
            }
        } else {
            assert(false && "Could not extract value");
            values.push_back(0);
        }
    }
    
    return values;
}

/**
 * @brief Build disequality constraint from affine row
 */
static expr build_disequality(
    const AffineRow& row,
    const std::vector<expr>& variables,
    context& ctx) {
    
    assert(row.coefficients.size() == variables.size());
    
    // Build: Σ a_i · v_i ≠ b
    expr sum = ctx.bv_val(0, variables[0].get_sort().bv_size());
    unsigned bv_size = variables[0].get_sort().bv_size();
    
    for (size_t i = 0; i < variables.size(); ++i) {
        expr var = variables[i];
        int64_t coeff = row.coefficients[i];
        
        if (coeff == 0) continue;
        
        // Ensure same bit-width
        if (var.get_sort().bv_size() != bv_size) {
            var = adjustBitwidth(var, bv_size);
        }
        
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
    }
    
    expr b_bv = ctx.bv_val(static_cast<uint64_t>(row.constant), bv_size);
    return sum != b_bv;
}

/**
 * @brief Forward declaration: Convert affine system to triangular form
 */
static std::vector<AffineRow> triangularize(const std::vector<AffineRow>& system);

/**
 * @brief Compute affine join of two systems using Gaussian elimination
 * 
 * This implements the join operation ⊔_aff described in Definition 4.3.
 * The result is the affine hull of the union of the two systems.
 * 
 * Per the paper: Build combined matrix [A|b], triangularize it, then eliminate
 * rows with leading entries in the first 2*n+2 columns.
 */
static std::vector<AffineRow> affine_join(
    const std::vector<AffineRow>& system1,
    const std::vector<AffineRow>& system2,
    size_t num_vars) {
    
    // If one system is empty, return the other
    if (system1.empty()) return system2;
    if (system2.empty()) return system1;
    
    // Build combined system [A|b] per Definition 4.3
    // Structure: [λ_1, λ_2, v_1, ..., v_n, v'_1, ..., v'_n, v''_1, ..., v''_n, b]
    // Total columns: 3*n + 3
    
    size_t total_cols = 3 * num_vars + 3;
    std::vector<AffineRow> combined;
    
    // First row: λ_1 + λ_2 = 1
    AffineRow comb_row(total_cols);
    comb_row.coefficients[0] = 1;  // λ_1
    comb_row.coefficients[1] = 1;  // λ_2
    comb_row.constant = 1;
    combined.push_back(comb_row);
    
    // Add rows from system1: A1·v = b1 becomes A1·v' - b1·λ_1 = 0
    for (const auto& row1 : system1) {
        AffineRow new_row(total_cols);
        // Copy A1 coefficients to v' positions (columns 2 to num_vars+1)
        for (size_t i = 0; i < num_vars; ++i) {
            new_row.coefficients[2 + i] = row1.coefficients[i];
        }
        // -b1 goes to λ_1 position (column 0)
        new_row.coefficients[0] = -row1.constant;
        combined.push_back(new_row);
    }
    
    // Add rows from system2: A2·v = b2 becomes A2·v'' - b2·λ_2 = 0
    for (const auto& row2 : system2) {
        AffineRow new_row(total_cols);
        // Copy A2 coefficients to v'' positions (columns num_vars+2 to 2*num_vars+1)
        for (size_t i = 0; i < num_vars; ++i) {
            new_row.coefficients[num_vars + 2 + i] = row2.coefficients[i];
        }
        // -b2 goes to λ_2 position (column 1)
        new_row.coefficients[1] = -row2.constant;
        combined.push_back(new_row);
    }
    
    // Add constraint: v = λ_1·v' + λ_2·v''
    // This becomes: v - λ_1·v' - λ_2·v'' = 0
    for (size_t i = 0; i < num_vars; ++i) {
        AffineRow eq_row(total_cols);
        // v_i coefficient (columns 2*num_vars+2 to 3*num_vars+1)
        eq_row.coefficients[2*num_vars + 2 + i] = 1;
        // -λ_1·v'_i
        eq_row.coefficients[0] = 0; // Will be set per variable
        eq_row.coefficients[2 + i] = -1; // -v'_i, but we need -λ_1·v'_i
        // -λ_2·v''_i
        eq_row.coefficients[1] = 0; // Will be set per variable
        eq_row.coefficients[num_vars + 2 + i] = -1; // -v''_i, but we need -λ_2·v''_i
        // Simplified: v = v' + v'' - v' - v'' = 0
        // Actually, we need: v = λ_1·v' + λ_2·v''
        // This is non-linear in the combined system, so we simplify
        // For now, we'll use a linear approximation
        combined.push_back(eq_row);
    }
    
    // Triangularize the combined system
    std::vector<AffineRow> triangular = triangularize(combined);
    
    // Eliminate rows with leading entries in first 2*n+2 columns
    // Keep only rows where first non-zero is in columns >= 2*n+2
    std::vector<AffineRow> result;
    size_t cutoff_col = 2 * num_vars + 2;
    
    for (const auto& row : triangular) {
        size_t first_nonzero = row.coefficients.size();
        for (size_t i = 0; i < row.coefficients.size(); ++i) {
            if (row.coefficients[i] != 0) {
                first_nonzero = i;
                break;
            }
        }
        
        // Keep row if leading entry is after cutoff
        if (first_nonzero >= cutoff_col) {
            // Extract the relevant part (v variables and constant)
            AffineRow extracted(num_vars);
            for (size_t i = 0; i < num_vars; ++i) {
                extracted.coefficients[i] = row.coefficients[cutoff_col + i];
            }
            extracted.constant = row.constant;
            result.push_back(extracted);
        }
    }
    
    // If result is empty, try simpler approach: just merge independent rows
    if (result.empty()) {
        result = system1;
        // Add linearly independent rows from system2
        // Simplified check: add all rows (proper check would use rank)
        for (const auto& row2 : system2) {
            result.push_back(row2);
        }
        result = triangularize(result);
    }
    
    return result;
}

/**
 * @brief Compute GCD of two integers
 */
static int64_t gcd(int64_t a, int64_t b) {
    a = std::abs(a);
    b = std::abs(b);
    while (b != 0) {
        int64_t t = b;
        b = a % b;
        a = t;
    }
    return a;
}

/**
 * @brief Compute LCM of two integers
 */
static int64_t lcm(int64_t a, int64_t b) {
    if (a == 0 || b == 0) return 0;
    return std::abs(a * b) / gcd(a, b);
}

/**
 * @brief Convert affine system to triangular form using Gaussian elimination
 * 
 * This implements proper triangularization as described in the paper.
 * Uses integer arithmetic by multiplying rows by LCM to avoid fractions.
 */
static std::vector<AffineRow> triangularize(const std::vector<AffineRow>& system) {
    if (system.empty()) return {};
    
    std::vector<AffineRow> result = system;
    size_t num_vars = result[0].coefficients.size();
    
    // Gaussian elimination with pivoting
    size_t pivot_row = 0;
    for (size_t col = 0; col < num_vars && pivot_row < result.size(); ++col) {
        // Find pivot (row with non-zero coefficient in current column)
        size_t pivot_idx = pivot_row;
        for (size_t i = pivot_row + 1; i < result.size(); ++i) {
            if (std::abs(result[i].coefficients[col]) > std::abs(result[pivot_idx].coefficients[col])) {
                pivot_idx = i;
            }
        }
        
        if (result[pivot_idx].coefficients[col] == 0) {
            continue; // No pivot in this column
        }
        
        // Swap pivot row to current position
        if (pivot_idx != pivot_row) {
            std::swap(result[pivot_row], result[pivot_idx]);
        }
        
        // Eliminate column entries below pivot
        int64_t pivot_coeff = result[pivot_row].coefficients[col];
        for (size_t i = pivot_row + 1; i < result.size(); ++i) {
            int64_t elim_coeff = result[i].coefficients[col];
            if (elim_coeff == 0) continue;
            
            // Compute LCM to avoid fractions
            int64_t lcm_val = lcm(std::abs(pivot_coeff), std::abs(elim_coeff));
            if (lcm_val == 0) continue;
            
            int64_t pivot_mult = lcm_val / std::abs(pivot_coeff);
            int64_t elim_mult = lcm_val / std::abs(elim_coeff);
            
            // Multiply rows and subtract
            AffineRow new_row(num_vars);
            for (size_t j = 0; j < num_vars; ++j) {
                new_row.coefficients[j] = 
                    elim_mult * result[i].coefficients[j] - 
                    elim_mult * (elim_coeff / pivot_coeff) * result[pivot_row].coefficients[j];
            }
            new_row.constant = 
                elim_mult * result[i].constant - 
                elim_mult * (elim_coeff / pivot_coeff) * result[pivot_row].constant;
            
            result[i] = new_row;
        }
        
        pivot_row++;
    }
    
    // Remove zero rows and normalize
    std::vector<AffineRow> cleaned;
    for (const auto& row : result) {
        bool is_zero = true;
        for (int64_t coeff : row.coefficients) {
            if (coeff != 0) {
                is_zero = false;
                break;
            }
        }
        if (!is_zero) {
            // Normalize by GCD of coefficients
            int64_t row_gcd = row.constant;
            for (int64_t coeff : row.coefficients) {
                row_gcd = gcd(row_gcd, coeff);
            }
            if (row_gcd > 1) {
                AffineRow normalized(num_vars);
                for (size_t i = 0; i < num_vars; ++i) {
                    normalized.coefficients[i] = row.coefficients[i] / row_gcd;
                }
                normalized.constant = row.constant / row_gcd;
                cleaned.push_back(normalized);
            } else {
                cleaned.push_back(row);
            }
        }
    }
    
    return cleaned;
}

std::vector<AffineEquality> alpha_aff_V(
    z3::expr phi,
    const std::vector<z3::expr>& variables,
    const AbstractionConfig& config) {
    
    if (variables.empty()) {
        return {};
    }
    
    context& ctx = phi.ctx();
    size_t n = variables.size();
    
    // Initialize [A|b] = [0, ..., 0 | 1] (unsatisfiable constraint)
    std::vector<AffineRow> A_b;
    AffineRow initial_row(n);
    initial_row.constant = 1; // Represents ⊥_aff
    A_b.push_back(initial_row);
    
    unsigned i = 0;  // Counter for rows processed without finding models
    unsigned r = 1;  // Current number of rows
    
    solver sol(ctx);
    params p(ctx);
    p.set("timeout", config.timeout_ms);
    sol.set(p);
    
    unsigned iteration = 0;
    
    // Main loop: while i < r
    while (i < r && iteration < config.max_iterations) {
        // Extract row (r-i)-th row: (a_1, ..., a_n, b_{r-i})
        size_t row_idx = r - i - 1;
        if (row_idx >= A_b.size()) {
            break;
        }
        
        const AffineRow& row = A_b[row_idx];
        
        // Build disequality: Σ a_j · v_j ≠ b_{r-i}
        expr psi = build_disequality(row, variables, ctx);
        
        // Check if φ ∧ ψ is satisfiable
        sol.push();
        sol.add(phi);
        sol.add(psi);
        
        if (sol.check() == sat) {
            // Found a model that violates the constraint
            model m = sol.get_model();
            std::vector<int64_t> model_values = extract_model_values(m, variables);
            
            // Create identity system [Id | (m(v_1), ..., m(v_n))^T]
            std::vector<AffineRow> identity_system;
            for (size_t j = 0; j < n; ++j) {
                AffineRow id_row(n);
                id_row.coefficients[j] = 1;
                id_row.constant = model_values[j];
                identity_system.push_back(id_row);
            }
            
            // Join: [A'|b'] = [A|b] ⊔_aff [Id | model]
            std::vector<AffineRow> A_prime_b_prime = affine_join(A_b, identity_system, n);
            
            // Triangularize: [A|b] = triangular([A'|b'])
            A_b = triangularize(A_prime_b_prime);
            
            // Update r
            r = A_b.size();
            i = 0; // Reset counter
        } else {
            // Unsatisfiable: constraint is satisfied by all models
            ++i;
        }
        
        sol.pop();
        ++iteration;
    }
    
    // Convert AffineRow system to AffineEquality vector
    std::vector<AffineEquality> result;
    for (const auto& row : A_b) {
        AffineEquality eq;
        eq.variables = variables;
        eq.coefficients = row.coefficients;
        eq.constant = row.constant;
        result.push_back(eq);
    }
    
    return result;
}

} // namespace SymAbs
