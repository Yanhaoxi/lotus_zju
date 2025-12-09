/**
 * @file Congruence.cpp
 * @brief Implementation of Algorithm 11: α_a-cong
 * 
 * Computes the least arithmetical congruence ⟨v⟩ ≡_m c describing values of v
 * that satisfy φ. The algorithm iteratively refines the modulus m using GCD.
 */

#include "Solvers/SMT/SymAbs/SymbolicAbstraction.h"
#include "Verification/SymbolicAbstraction/Utils/Z3APIExtension.h"
#include <z3++.h>
#include <z3.h>
#include <numeric>
#include <cassert>
#include <cmath>

using namespace z3;

namespace SymAbs {

/**
 * @brief Compute GCD of two integers
 */
static uint64_t gcd(uint64_t a, uint64_t b) {
    while (b != 0) {
        uint64_t t = b;
        b = a % b;
        a = t;
    }
    return a;
}

/**
 * @brief Extract integer value from model
 */
static int64_t get_model_value(const model& m, const expr& var) {
    z3::expr val = m.eval(var, true);
    
    if (val.is_numeral()) {
        // Get as signed integer
        std::string num_str = Z3_get_numeral_string(val.ctx(), val);
        return std::stoll(num_str);
    }
    
    // If not a numeral, try to interpret as bit-vector
    if (val.is_bv()) {
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
                    signed_val = signed_val | ~mask; // Sign extend
                }
                return signed_val;
            }
            return static_cast<int64_t>(unsigned_val);
        }
    }
    
    assert(false && "Could not extract integer value from model");
    return 0;
}

/**
 * @brief Convert signed bit-vector to integer.
 */
static z3::expr bv_signed_to_int(const z3::expr& bv) {
    z3::context& ctx = bv.ctx();
    unsigned w = bv.get_sort().bv_size();
    z3::expr msb = z3_ext::extract(w - 1, w - 1, bv);
     z3::expr unsigned_val = to_expr(ctx, Z3_mk_bv2int(ctx, bv, false));
    int64_t two_pow_w_val = 1LL << static_cast<int>(w);
    z3::expr two_pow_w = ctx.int_val(two_pow_w_val);
    return z3::ite(msb == ctx.bv_val(1, 1), unsigned_val - two_pow_w, unsigned_val);
}

llvm::Optional<Congruence> alpha_a_cong(
    z3::expr phi,
    z3::expr variable,
    const AbstractionConfig& config) {
    
    context& ctx = phi.ctx();
    solver sol(ctx);
    
    params p(ctx);
    p.set("timeout", config.timeout_ms);
    sol.set(p);
    
    // Initialize: (m, c) = (0, ⊥)
    uint64_t m = 0;
    llvm::Optional<int64_t> c_opt;
    z3::expr current_phi = phi;
    unsigned iteration = 0;

    while (iteration < config.max_iterations) {
        sol.push();
        sol.add(current_phi);

        if (sol.check() != sat) {
            sol.pop();
            break;
        }

        model m_model = sol.get_model();
        int64_t v_val = get_model_value(m_model, variable);

        if (!c_opt.hasValue()) {
            c_opt = v_val;
            m = 0; // singleton so far
        } else {
            int64_t c_val = c_opt.getValue();
            int64_t d = std::abs(v_val - c_val);
            if (d == 0) {
                // Already captured; exclude this specific value
                unsigned bv_size = variable.get_sort().bv_size();
                current_phi = current_phi && (variable != ctx.bv_val(static_cast<uint64_t>(v_val), bv_size));
                sol.pop();
                ++iteration;
                continue;
            }

            m = (m == 0) ? static_cast<uint64_t>(d) : gcd(m, static_cast<uint64_t>(d));
        }

        sol.pop();

        // Exclude current congruence class so we can discover new witnesses.
        if (m == 0) {
            unsigned bv_size = variable.get_sort().bv_size();
            current_phi = current_phi && (variable != ctx.bv_val(static_cast<uint64_t>(c_opt.getValue()), bv_size));
        } else {
            z3::expr v_int = bv_signed_to_int(variable);
            z3::expr c_int = ctx.int_val(c_opt.getValue());
            z3::expr mod_val = z3::mod(v_int - c_int, ctx.int_val(static_cast<int64_t>(m)));
            current_phi = current_phi && (mod_val != ctx.int_val(0));
        }

        ++iteration;
    }
    
    if (!c_opt.hasValue()) {
        return llvm::None; // No models found
    }
    
    // Normalize: c ← c mod m
    int64_t c_normalized = c_opt.getValue();
    if (m > 0) {
        int64_t c_mod = c_normalized % static_cast<int64_t>(m);
        if (c_mod < 0) c_mod += static_cast<int64_t>(m);
        c_normalized = c_mod;
    }

    return Congruence(variable, m, c_normalized);
}

} // namespace SymAbs
