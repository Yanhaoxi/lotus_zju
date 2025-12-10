/**
 * @file SymAbsUtils.cpp
 * @brief Implementation of utility functions for symbolic abstraction
 */

#include "Solvers/SMT/SymAbs/SymAbsUtils.h"
#include "Verification/SymbolicAbstraction/Utils/Z3APIExtension.h"
#include <z3++.h>
#include <z3.h>
#include <cassert>
#include <algorithm>
#include <numeric>

using namespace z3;

namespace SymAbs {

// ============================================================================
// Bit-vector to integer conversion
// ============================================================================

expr bv_signed_to_int(const expr& bv) {
    context& ctx = bv.ctx();
    const unsigned w = bv.get_sort().bv_size();
    expr msb = z3_ext::extract(w - 1, w - 1, bv);
    expr unsigned_val = to_expr(ctx, Z3_mk_bv2int(ctx, bv, false));
    int64_t two_pow_w_val = 1LL << static_cast<int>(w);
    expr two_pow_w = ctx.int_val(two_pow_w_val);
    return ite(msb == ctx.bv_val(1, 1), unsigned_val - two_pow_w, unsigned_val);
}

// ============================================================================
// Integer extraction from Z3 expressions
// ============================================================================

llvm::Optional<int64_t> to_int64(const expr& val) {
    if (!val.is_numeral()) {
        return llvm::None;
    }
    int64_t out = 0;
    if (Z3_get_numeral_int64(val.ctx(), val, &out)) {
        return out;
    }
    // Fallback through string conversion if the helper fails (e.g., big values)
    try {
        std::string s = Z3_get_numeral_string(val.ctx(), val);
        return std::stoll(s);
    } catch (...) {
        return llvm::None;
    }
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

// ============================================================================
// Arithmetic utilities
// ============================================================================

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

int64_t lcm64(int64_t a, int64_t b) {
    if (a == 0 || b == 0) {
        return 0;
    }
    return std::abs(a / gcd64(a, b) * b);
}

int64_t div_floor(int64_t num, int64_t denom) {
    assert(denom > 0 && "denominator must be positive");
    if (num >= 0) {
        return num / denom;
    }
    return -static_cast<int64_t>((-num + denom - 1) / denom);
}

// ============================================================================
// Rational number class
// ============================================================================

void Rational::normalize() {
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

Rational::Rational() : num_(0), den_(1) {}

Rational::Rational(int64_t n) : num_(n), den_(1) {}

Rational::Rational(int64_t n, int64_t d) : num_(n), den_(d) {
    normalize();
}

Rational Rational::operator+(const Rational& other) const {
    int64_t n = num_ * other.den_ + other.num_ * den_;
    int64_t d = den_ * other.den_;
    return Rational(n, d);
}

Rational Rational::operator-(const Rational& other) const {
    int64_t n = num_ * other.den_ - other.num_ * den_;
    int64_t d = den_ * other.den_;
    return Rational(n, d);
}

Rational Rational::operator*(const Rational& other) const {
    int64_t n = num_ * other.num_;
    int64_t d = den_ * other.den_;
    return Rational(n, d);
}

Rational Rational::operator/(const Rational& other) const {
    int64_t n = num_ * other.den_;
    int64_t d = den_ * other.num_;
    return Rational(n, d);
}

Rational& Rational::operator/=(const Rational& other) {
    *this = *this / other;
    return *this;
}

Rational& Rational::operator-=(const Rational& other) {
    *this = *this - other;
    return *this;
}

Rational Rational::operator-() const {
    return Rational(-num_, den_);
}

bool Rational::operator==(const Rational& other) const {
    return num_ == other.num_ && den_ == other.den_;
}

bool Rational::operator!=(const Rational& other) const {
    return !(*this == other);
}

// ============================================================================
// Matrix operations
// ============================================================================

RrefResult rref(const std::vector<std::vector<Rational>>& input) {
    if (input.empty()) {
        return {};
    }

    std::vector<std::vector<Rational>> m = input;
    const size_t rows = m.size();
    const size_t cols = m[0].size();
    std::vector<size_t> pivot_cols;

    size_t r = 0;
    for (size_t c = 0; c < cols && r < rows; ++c) {
        size_t pivot = r;
        while (pivot < rows && m[pivot][c] == Rational(0)) {
            ++pivot;
        }
        if (pivot == rows) {
            continue;
        }
        std::swap(m[r], m[pivot]);

        Rational pivot_val = m[r][c];
        for (size_t k = c; k < cols; ++k) {
            m[r][k] /= pivot_val;
        }

        for (size_t i = 0; i < rows; ++i) {
            if (i == r) continue;
            Rational factor = m[i][c];
            if (factor == Rational(0)) continue;
            for (size_t k = c; k < cols; ++k) {
                m[i][k] -= factor * m[r][k];
            }
        }

        pivot_cols.push_back(c);
        ++r;
    }

    return {m, pivot_cols};
}

} // namespace SymAbs
