/**
 * @file Polyhedron.cpp
 * @brief Implementation of Algorithm 9: α_conv^V
 *
 * Computes a convex polyhedron over-approximating φ by iteratively finding
 * extremal points (vertices) and computing their convex hull. Compared to the
 * previous bounding-box implementation, this version builds supporting
 * half-spaces from discovered vertices, preserving affine dependencies.
 */

#include "Solvers/SMT/SymAbs/SymbolicAbstraction.h"
#include "Solvers/SMT/SymAbs/SymAbsUtils.h"
#include "Verification/SymbolicAbstraction/Utils/Z3APIExtension.h"
#include <z3++.h>
#include <z3.h>

#include <algorithm>
#include <cassert>
#include <numeric>
#include <sstream>
#include <unordered_set>
#include <vector>

using namespace z3;

namespace SymAbs {
namespace {

static expr build_inequality(const AffineEquality &ineq, const std::vector<expr> &variables,
                             context &ctx) {
    assert(ineq.coefficients.size() == variables.size());
    expr lhs = ctx.int_val(0);
    for (size_t i = 0; i < variables.size(); ++i) {
        if (ineq.coefficients[i] == 0)
            continue;
        lhs = lhs + ctx.int_val(ineq.coefficients[i]) * SymAbs::bv_signed_to_int(variables[i]);
    }
    expr rhs = ctx.int_val(ineq.constant);
    return lhs <= rhs;
}

static expr build_not_poly(const std::vector<AffineEquality> &poly,
                           const std::vector<expr> &variables, context &ctx) {
    if (poly.empty()) {
        return ctx.bool_val(true);
    }

    expr_vector violations(ctx);
    for (const auto &ineq : poly) {
        violations.push_back(!build_inequality(ineq, variables, ctx));
    }

    return mk_or(violations);
}


// Compute a candidate facet normal from a subset of n points (dimension n).
static llvm::Optional<std::vector<int64_t>>
compute_normal_from_subset(const std::vector<std::vector<int64_t>> &subset) {
    if (subset.empty())
        return llvm::None;
    const size_t n = subset.front().size();
    if (subset.size() != n) {
        return llvm::None;
    }

    // Build matrix of difference vectors (n-1 rows, n columns).
    std::vector<std::vector<Rational>> diff;
    diff.reserve(n - 1);
    for (size_t i = 1; i < subset.size(); ++i) {
        std::vector<Rational> row;
        row.reserve(n);
        for (size_t j = 0; j < n; ++j) {
            row.emplace_back(subset[i][j] - subset[0][j]);
        }
        diff.push_back(std::move(row));
    }

    RrefResult rr = SymAbs::rref(diff);
    const auto &mat = rr.matrix;
    const auto &pivots = rr.pivot_columns;

    std::vector<size_t> free_cols;
    for (size_t c = 0; c < n; ++c) {
        if (std::find(pivots.begin(), pivots.end(), c) == pivots.end()) {
            free_cols.push_back(c);
        }
    }

    if (free_cols.size() != 1) {
        // Nullspace dimension not equal to 1 -> not a unique facet.
        return llvm::None;
    }

    size_t free_col = free_cols.front();
    std::vector<Rational> basis(n, Rational(0));
    basis[free_col] = Rational(1);

    for (size_t row = 0; row < mat.size() && row < pivots.size(); ++row) {
        size_t pcol = pivots[row];
        basis[pcol] = -mat[row][free_col];
    }

    int64_t scale = 1;
    for (const auto &v : basis) {
        scale = SymAbs::lcm64(scale, v.denominator());
    }
    if (scale == 0) {
        return llvm::None;
    }

    std::vector<int64_t> normal(n, 0);
    for (size_t i = 0; i < n; ++i) {
        normal[i] = static_cast<int64_t>(basis[i].numerator() * (scale / basis[i].denominator()));
    }

    return normal;
}

static bool normalize(std::vector<int64_t> &normal, int64_t &bound) {
    int64_t g = 0;
    for (auto c : normal) {
        g = SymAbs::gcd64(g, c);
    }
    if (g == 0) {
        return false;
    }
    for (auto &c : normal) {
        c /= g;
    }
    bound /= g;

    // Fix orientation: make the first non-zero coefficient positive.
    for (auto c : normal) {
        if (c == 0)
            continue;
        if (c < 0) {
            for (auto &v : normal)
                v = -v;
            bound = -bound;
        }
        break;
    }
    return true;
}

static int64_t dot(const std::vector<int64_t> &a, const std::vector<int64_t> &b) {
    assert(a.size() == b.size());
    int64_t res = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        res += a[i] * b[i];
    }
    return res;
}

// Build supporting half-spaces of the convex hull of the given points.
static std::vector<AffineEquality>
convex_hull_halfspaces(const std::vector<std::vector<int64_t>> &points,
                       const std::vector<expr> &variables) {
    std::vector<AffineEquality> constraints;
    if (points.empty()) {
        return constraints;
    }

    const size_t n = variables.size();

    // Per-variable bounds keep the hull closed and inexpensive to compute.
    for (size_t i = 0; i < n; ++i) {
        int64_t min_v = points.front()[i];
        int64_t max_v = points.front()[i];
        for (const auto &p : points) {
            min_v = std::min(min_v, p[i]);
            max_v = std::max(max_v, p[i]);
        }
        AffineEquality lower;
        lower.variables = variables;
        lower.coefficients.assign(n, 0);
        lower.coefficients[i] = -1;
        lower.constant = -min_v;
        constraints.push_back(lower);

        AffineEquality upper;
        upper.variables = variables;
        upper.coefficients.assign(n, 0);
        upper.coefficients[i] = 1;
        upper.constant = max_v;
        constraints.push_back(upper);
    }

    if (n == 1 || points.size() == 1) {
        return constraints;
    }

    std::unordered_set<std::string> seen;
    std::vector<size_t> combo;

    auto emit_key = [](const std::vector<int64_t> &norm, int64_t bound) {
        std::ostringstream oss;
        for (auto c : norm) {
            oss << c << ",";
        }
        oss << "|" << bound;
        return oss.str();
    };

    std::function<void(size_t, size_t)> choose = [&](size_t start, size_t need) {
        if (need == 0) {
            std::vector<std::vector<int64_t>> subset;
            subset.reserve(combo.size());
            for (auto idx : combo) {
                subset.push_back(points[idx]);
            }

            auto normal_opt = compute_normal_from_subset(subset);
            if (!normal_opt.hasValue()) {
                return;
            }
            auto normal = normal_opt.getValue();
            int64_t bound = dot(normal, subset.front());
            if (!normalize(normal, bound)) {
                return;
            }

            int64_t min_dot = bound;
            int64_t max_dot = bound;
            for (const auto &p : points) {
                int64_t v = dot(normal, p);
                min_dot = std::min(min_dot, v);
                max_dot = std::max(max_dot, v);
            }

            auto add_constraint = [&](const std::vector<int64_t> &norm, int64_t b) {
                auto key = emit_key(norm, b);
                if (seen.insert(key).second) {
                    AffineEquality ineq;
                    ineq.variables = variables;
                    ineq.coefficients = norm;
                    ineq.constant = b;
                    constraints.push_back(std::move(ineq));
                }
            };

            if (min_dot == max_dot) {
                add_constraint(normal, bound);
                std::vector<int64_t> neg = normal;
                int64_t neg_b = -bound;
                for (auto &c : neg)
                    c = -c;
                add_constraint(neg, neg_b);
            } else if (max_dot <= bound) {
                add_constraint(normal, bound);
            } else if (min_dot >= bound) {
                std::vector<int64_t> neg = normal;
                int64_t neg_b = -bound;
                for (auto &c : neg)
                    c = -c;
                add_constraint(neg, neg_b);
            }
            return;
        }

        for (size_t i = start; i + need <= points.size(); ++i) {
            combo.push_back(i);
            choose(i + 1, need - 1);
            combo.pop_back();
        }
    };

    choose(0, n);
    return constraints;
}

} // namespace

std::vector<AffineEquality> alpha_conv_V(z3::expr phi, const std::vector<z3::expr> &variables,
                                         const AbstractionConfig &config) {
    if (variables.empty()) {
        return {};
    }

    context &ctx = phi.ctx();
    params p(ctx);
    p.set("timeout", config.timeout_ms);

    solver init(ctx);
    init.set(p);
    init.add(phi);
    if (init.check() != sat) {
        return {};
    }

    std::vector<std::vector<int64_t>> points;
    points.push_back(SymAbs::extract_point(init.get_model(), variables));

    std::vector<AffineEquality> polyhedron = convex_hull_halfspaces(points, variables);

    unsigned iteration = 0;
    while (iteration < config.max_iterations &&
           polyhedron.size() < config.max_inequalities) {
        expr not_c = build_not_poly(polyhedron, variables, ctx);

        solver witness(ctx);
        witness.set(p);
        witness.add(phi);
        witness.add(not_c);
        if (witness.check() != sat) {
            break; // φ ⊨ c
        }

        std::vector<std::vector<int64_t>> new_points;
        for (size_t v_idx = 0; v_idx < variables.size(); ++v_idx) {
            auto min_val = minimum(phi && not_c, variables[v_idx], config);
            auto max_val = maximum(phi && not_c, variables[v_idx], config);

            auto add_model_point = [&](int64_t value) {
                solver s(ctx);
                s.set(p);
                s.add(phi);
                s.add(not_c);
                unsigned bv_size = variables[v_idx].get_sort().bv_size();
                s.add(variables[v_idx] == ctx.bv_val(static_cast<uint64_t>(value), bv_size));
                if (s.check() == sat) {
                    new_points.push_back(SymAbs::extract_point(s.get_model(), variables));
                }
            };

            if (min_val.hasValue()) {
                add_model_point(min_val.getValue());
            }
            if (max_val.hasValue()) {
                add_model_point(max_val.getValue());
            }
        }

        for (const auto &pt : new_points) {
            if (std::find(points.begin(), points.end(), pt) == points.end()) {
                points.push_back(pt);
            }
        }

        auto updated = convex_hull_halfspaces(points, variables);
        if (!updated.empty()) {
            polyhedron = std::move(updated);
        }

        ++iteration;
    }

    return polyhedron;
}

std::vector<AffineEquality> relax_conv(z3::expr phi,
                                       const std::vector<AffineEquality> &template_constraints,
                                       const AbstractionConfig &config) {
    if (template_constraints.empty()) {
        return {};
    }

    std::vector<AffineEquality> result;
    result.reserve(template_constraints.size());

    for (const auto &templ : template_constraints) {
        LinearExpression lexpr;
        lexpr.variables = templ.variables;
        lexpr.coefficients = templ.coefficients;

        auto bound = alpha_lin_exp(phi, lexpr, config);
        if (!bound.hasValue()) {
            continue;
        }

        AffineEquality relaxed = templ;
        relaxed.constant = bound.getValue();
        result.push_back(std::move(relaxed));
    }

    return result;
}

} // namespace SymAbs
