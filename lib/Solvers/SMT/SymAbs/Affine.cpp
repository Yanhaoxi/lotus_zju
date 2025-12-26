/**
 * @file Affine.cpp
 * @brief Implementation of α_aff^V
 *
 * Computes the affine hull of the models of φ. We iteratively look for models
 * that violate the current hull and rebuild the hull from all collected models
 * using exact rational arithmetic to avoid loss of precision.
 */

#include "Solvers/SMT/SymAbs/SymbolicAbstraction.h"
#include "Solvers/SMT/SymAbs/SymAbsUtils.h"
#include "Verification/SymbolicAbstraction/Utils/Z3APIExtension.h"

#include <algorithm>
#include <cassert>
#include <numeric>
#include <z3++.h>
#include <z3.h>

using namespace z3;

namespace SymAbs {
namespace {

using Rat = Rational;

/**
 * Given a set of points, compute the affine equalities describing their hull.
 */
std::vector<AffineEquality> build_equalities_from_points(
    const std::vector<std::vector<int64_t>>& points,
    const std::vector<expr>& variables) {

    std::vector<AffineEquality> result;
    if (points.empty()) {
        return result;
    }

    const size_t n = variables.size();
    const auto& anchor = points.front();

    if (points.size() == 1) {
        for (size_t i = 0; i < n; ++i) {
            AffineEquality eq;
            eq.variables = variables;
            eq.coefficients.assign(n, 0);
            eq.coefficients[i] = 1;
            eq.constant = anchor[i];
            result.push_back(eq);
        }
        return result;
    }

    // Build matrix of difference vectors.
    std::vector<std::vector<Rat>> diff;
    diff.reserve(points.size() - 1);
    for (size_t idx = 1; idx < points.size(); ++idx) {
        std::vector<Rat> row;
        row.reserve(n);
        for (size_t j = 0; j < n; ++j) {
            row.emplace_back(points[idx][j] - anchor[j]);
        }
        diff.push_back(std::move(row));
    }

    RrefResult rref_res = SymAbs::rref(diff);
    const auto& mat = rref_res.matrix;
    const auto& pivots = rref_res.pivot_columns;

    // Identify free columns.
    std::vector<size_t> free_cols;
    for (size_t c = 0; c < n; ++c) {
        if (std::find(pivots.begin(), pivots.end(), c) == pivots.end()) {
            free_cols.push_back(c);
        }
    }

    if (free_cols.empty()) {
        // Full rank: only the anchor point is possible.
        for (size_t i = 0; i < n; ++i) {
            AffineEquality eq;
            eq.variables = variables;
            eq.coefficients.assign(n, 0);
            eq.coefficients[i] = 1;
            eq.constant = anchor[i];
            result.push_back(eq);
        }
        return result;
    }

    // Build basis for the nullspace.
    for (size_t free_col : free_cols) {
        std::vector<Rat> basis(n, Rat(0));
        basis[free_col] = Rat(1);

        for (size_t row = 0; row < mat.size() && row < pivots.size(); ++row) {
            size_t pcol = pivots[row];
            basis[pcol] = -mat[row][free_col];
        }

        // Scale to integer coefficients.
        int64_t scale = 1;
        for (const auto& v : basis) {
            scale = SymAbs::lcm64(scale, v.denominator());
        }
        if (scale == 0) continue;

        std::vector<int64_t> coeffs(n, 0);
        for (size_t i = 0; i < n; ++i) {
            coeffs[i] = static_cast<int64_t>(basis[i].numerator() * (scale / basis[i].denominator()));
        }

        // Normalize by GCD.
        int64_t g = 0;
        for (int64_t c : coeffs) {
            g = SymAbs::gcd64(g, c);
        }
        if (g == 0) {
            continue;
        }
        for (auto& c : coeffs) {
            c /= g;
        }

        int64_t constant = 0;
        for (size_t i = 0; i < n; ++i) {
            constant += coeffs[i] * anchor[i];
        }

        AffineEquality eq;
        eq.variables = variables;
        eq.coefficients = std::move(coeffs);
        eq.constant = constant;
        result.push_back(std::move(eq));
    }

    return result;
}

expr equality_to_expr(const AffineEquality& eq, const std::vector<expr>& vars) {
    context& ctx = vars.front().ctx();
    assert(eq.coefficients.size() == vars.size());

    expr lhs = ctx.int_val(0);
    for (size_t i = 0; i < vars.size(); ++i) {
        if (eq.coefficients[i] == 0) continue;
        lhs = lhs + ctx.int_val(eq.coefficients[i]) * SymAbs::bv_signed_to_int(vars[i]);
    }
    expr rhs = ctx.int_val(eq.constant);
    return lhs == rhs;
}

} // namespace

std::vector<AffineEquality> alpha_aff_V(
    z3::expr phi,
    const std::vector<z3::expr>& variables,
    const AbstractionConfig& config) {

    if (variables.empty()) {
        return {};
    }

    context& ctx = phi.ctx();
    solver init(ctx);
    params p(ctx);
    p.set("timeout", config.timeout_ms);
    init.set(p);
    init.add(phi);

    if (init.check() != sat) {
        return {};
    }

    model m0 = init.get_model();
    std::vector<std::vector<int64_t>> points;
    points.push_back(SymAbs::extract_point(m0, variables));

    unsigned iteration = 0;
    while (iteration < config.max_iterations) {
        auto equalities = build_equalities_from_points(points, variables);
        if (equalities.empty()) {
            return equalities;
        }

        expr_vector hull_conj(ctx);
        for (const auto& eq : equalities) {
            hull_conj.push_back(equality_to_expr(eq, variables));
        }
        expr hull = mk_and(hull_conj);

        solver refine(ctx);
        refine.set(p);
        refine.add(phi);
        refine.add(!hull);

        auto res = refine.check();
        if (res != sat) {
            return equalities;
        }

        model m_new = refine.get_model();
        auto new_point = SymAbs::extract_point(m_new, variables);
        if (std::find(points.begin(), points.end(), new_point) == points.end()) {
            points.push_back(std::move(new_point));
        } else {
            // No progress; return current hull.
            return equalities;
        }

        ++iteration;
    }

    return build_equalities_from_points(points, variables);
}

} // namespace SymAbs
