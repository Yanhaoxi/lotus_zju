/**
 * @file Affine.cpp
 * @brief Implementation of Algorithm 12: α_aff^V
 *
 * Computes the affine hull of the models of φ. We iteratively look for models
 * that violate the current hull and rebuild the hull from all collected models
 * using exact rational arithmetic to avoid loss of precision.
 */

#include "Solvers/SMT/SymAbs/SymbolicAbstraction.h"
#include "Verification/SymbolicAbstraction/Utils/Z3APIExtension.h"
#include <z3++.h>
#include <z3.h>
#include <algorithm>
#include <cassert>
#include <numeric>

using namespace z3;

namespace SymAbs {
namespace {

int64_t gcd64(int64_t a, int64_t b) {
    a = std::abs(a);
    b = std::abs(b);
    while (b != 0) {
        int64_t temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}

// Simple rational number class to replace boost::rational
class Rational {
    int64_t num_;
    int64_t den_;

    void normalize() {
        if (den_ == 0) {
            num_ = 0;
            den_ = 1;
            return;
        }
        if (den_ < 0) {
            num_ = -num_;
            den_ = -den_;
        }
        int64_t g = gcd64(std::abs(num_), den_);
        if (g > 1) {
            num_ /= g;
            den_ /= g;
        }
    }

public:
    Rational() : num_(0), den_(1) {}
    Rational(int64_t n) : num_(n), den_(1) {}
    Rational(int64_t n, int64_t d) : num_(n), den_(d) {
        normalize();
    }

    int64_t numerator() const { return num_; }
    int64_t denominator() const { return den_; }

    Rational operator+(const Rational& other) const {
        int64_t n = num_ * other.den_ + other.num_ * den_;
        int64_t d = den_ * other.den_;
        return Rational(n, d);
    }

    Rational operator-(const Rational& other) const {
        int64_t n = num_ * other.den_ - other.num_ * den_;
        int64_t d = den_ * other.den_;
        return Rational(n, d);
    }

    Rational operator*(const Rational& other) const {
        int64_t n = num_ * other.num_;
        int64_t d = den_ * other.den_;
        return Rational(n, d);
    }

    Rational operator/(const Rational& other) const {
        int64_t n = num_ * other.den_;
        int64_t d = den_ * other.num_;
        return Rational(n, d);
    }

    Rational& operator/=(const Rational& other) {
        *this = *this / other;
        return *this;
    }

    Rational& operator-=(const Rational& other) {
        *this = *this - other;
        return *this;
    }

    Rational operator-() const {
        return Rational(-num_, den_);
    }

    bool operator==(const Rational& other) const {
        return num_ == other.num_ && den_ == other.den_;
    }

    bool operator!=(const Rational& other) const {
        return !(*this == other);
    }
};

using Rat = Rational;

expr bv_signed_to_int(const expr& bv) {
    context& ctx = bv.ctx();
    const unsigned w = bv.get_sort().bv_size();
    expr msb = z3_ext::extract(w - 1, w - 1, bv);
    expr unsigned_val = to_expr(ctx, Z3_mk_bv2int(ctx, bv, false));
    const int64_t two_pow_w_val = 1LL << static_cast<int>(w);
    expr two_pow_w = ctx.int_val(two_pow_w_val);
    return ite(msb == ctx.bv_val(1, 1), unsigned_val - two_pow_w, unsigned_val);
}

bool eval_model_value(const model& m, const expr& v, int64_t& out) {
    expr val = m.eval(v, true);
    if (val.is_numeral()) {
        std::string num_str = Z3_get_numeral_string(val.ctx(), val);
        try {
            out = std::stoll(num_str);
            return true;
        } catch (...) {
            return false;
        }
    }

    if (val.is_bv()) {
        unsigned bv_size = val.get_sort().bv_size();
        if (bv_size > 64) {
            return false;
        }
        uint64_t unsigned_val = 0;
        if (!Z3_get_numeral_uint64(val.ctx(), val, &unsigned_val)) {
            return false;
        }
        int64_t signed_val = static_cast<int64_t>(unsigned_val);
        if (bv_size < 64) {
            const int64_t mask = (1LL << bv_size) - 1;
            const int64_t sign_mask = 1LL << (bv_size - 1);
            if (signed_val & sign_mask) {
                signed_val |= ~mask;
            }
        }
        out = signed_val;
        return true;
    }

    return false;
}

std::vector<int64_t> extract_point(const model& m, const std::vector<expr>& vars) {
    std::vector<int64_t> point;
    point.reserve(vars.size());
    for (const auto& v : vars) {
        int64_t val = 0;
        bool ok = eval_model_value(m, v, val);
        assert(ok && "Failed to extract model value");
        if (!ok) {
            val = 0;
        }
        point.push_back(val);
    }
    return point;
}

int64_t lcm64(int64_t a, int64_t b) {
    if (a == 0 || b == 0) {
        return 0;
    }
    return std::abs(a / gcd64(a, b) * b);
}

struct RrefResult {
    std::vector<std::vector<Rat>> matrix;
    std::vector<size_t> pivot_columns;
};

/**
 * Compute RREF of the given matrix (rows x cols) over rationals.
 */
RrefResult rref(const std::vector<std::vector<Rat>>& input) {
    if (input.empty()) {
        return {};
    }

    std::vector<std::vector<Rat>> m = input;
    const size_t rows = m.size();
    const size_t cols = m[0].size();
    std::vector<size_t> pivot_cols;

    size_t r = 0;
    for (size_t c = 0; c < cols && r < rows; ++c) {
        size_t pivot = r;
        while (pivot < rows && m[pivot][c] == Rat(0)) {
            ++pivot;
        }
        if (pivot == rows) {
            continue;
        }
        std::swap(m[r], m[pivot]);

        Rat pivot_val = m[r][c];
        for (size_t k = c; k < cols; ++k) {
            m[r][k] /= pivot_val;
        }

        for (size_t i = 0; i < rows; ++i) {
            if (i == r) continue;
            Rat factor = m[i][c];
            if (factor == Rat(0)) continue;
            for (size_t k = c; k < cols; ++k) {
                m[i][k] -= factor * m[r][k];
            }
        }

        pivot_cols.push_back(c);
        ++r;
    }

    return {m, pivot_cols};
}

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

    RrefResult rref_res = rref(diff);
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
            scale = lcm64(scale, v.denominator());
        }
        if (scale == 0) continue;

        std::vector<int64_t> coeffs(n, 0);
        for (size_t i = 0; i < n; ++i) {
            coeffs[i] = static_cast<int64_t>(basis[i].numerator() * (scale / basis[i].denominator()));
        }

        // Normalize by GCD.
        int64_t g = 0;
        for (int64_t c : coeffs) {
            g = gcd64(g, c);
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
        lhs = lhs + ctx.int_val(eq.coefficients[i]) * bv_signed_to_int(vars[i]);
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
    points.push_back(extract_point(m0, variables));

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
        auto new_point = extract_point(m_new, variables);
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
