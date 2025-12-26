/**
 * @file Congruence.cpp
 * @brief Implementation of Algorithm 11: α_a-cong
 * 
 * Computes the least arithmetical congruence ⟨v⟩ ≡_m c describing values of v
 * that satisfy φ. The algorithm iteratively refines the modulus m using GCD.
 */

#include "Solvers/SMT/SymAbs/SymbolicAbstraction.h"
#include "Solvers/SMT/SymAbs/SymAbsUtils.h"
#include "Verification/SymbolicAbstraction/Utils/Z3APIExtension.h"

#include <cassert>
#include <cmath>
#include <numeric>
#include <z3++.h>
#include <z3.h>

using namespace z3;

namespace SymAbs {

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
        int64_t v_val = 0;
        bool ok = eval_model_value(m_model, variable, v_val);
        assert(ok && "Failed to extract model value");

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

            m = (m == 0) ? static_cast<uint64_t>(d) : static_cast<uint64_t>(gcd64(m, static_cast<int64_t>(d)));
        }

        sol.pop();

        // Exclude current congruence class so we can discover new witnesses.
        if (m == 0) {
            unsigned bv_size = variable.get_sort().bv_size();
            current_phi = current_phi && (variable != ctx.bv_val(static_cast<uint64_t>(c_opt.getValue()), bv_size));
        } else {
            z3::expr v_int = SymAbs::bv_signed_to_int(variable);
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
