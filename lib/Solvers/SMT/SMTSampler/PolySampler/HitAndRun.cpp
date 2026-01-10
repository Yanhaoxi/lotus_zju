/**
 * @file HitAndRun.cpp
 * @brief Integer hit-and-run walk for linear constraints
 */

#include "Solvers/SMT/SMTSampler/PolySampler/HitAndRun.h"

#include <cmath>
#include <limits>

namespace RegionSampling {
namespace {

static long double dot_ld(const std::vector<int64_t> &a,
                          const std::vector<int64_t> &b) {
  long double sum = 0.0L;
  for (size_t i = 0; i < a.size(); ++i) {
    sum += static_cast<long double>(a[i]) *
           static_cast<long double>(b[i]);
  }
  return sum;
}

} // namespace

bool hit_and_run_step(const std::vector<LinearConstraint> &constraints,
                      std::vector<int64_t> &point,
                      std::mt19937_64 &rng) {
  if (point.empty())
    return false;

  std::normal_distribution<double> normal(0.0, 1.0);
  std::vector<int64_t> direction(point.size(), 0);
  for (int attempt = 0; attempt < 16; ++attempt) {
    bool non_zero = false;
    for (size_t i = 0; i < point.size(); ++i) {
      int64_t v = static_cast<int64_t>(std::llround(normal(rng)));
      direction[i] = v;
      if (v != 0)
        non_zero = true;
    }
    if (non_zero)
      break;
  }

  bool all_zero = true;
  for (auto v : direction) {
    if (v != 0) {
      all_zero = false;
      break;
    }
  }
  if (all_zero)
    return false;

  long double t_min = -std::numeric_limits<long double>::infinity();
  long double t_max = std::numeric_limits<long double>::infinity();

  for (const auto &c : constraints) {
    long double a_dot_x = dot_ld(c.coeffs, point);
    long double a_dot_d = dot_ld(c.coeffs, direction);
    long double slack = static_cast<long double>(c.bound) - a_dot_x;

    if (a_dot_d > 0.0L) {
      t_max = std::min(t_max, slack / a_dot_d);
    } else if (a_dot_d < 0.0L) {
      t_min = std::max(t_min, slack / a_dot_d);
    } else if (slack < 0.0L) {
      return false;
    }
  }

  if (!std::isfinite(t_min) || !std::isfinite(t_max) || t_min > t_max)
    return false;

  long double t_low_ld = std::ceil(t_min);
  long double t_high_ld = std::floor(t_max);
  if (t_low_ld > t_high_ld)
    return false;

  int64_t t_low = static_cast<int64_t>(t_low_ld);
  int64_t t_high = static_cast<int64_t>(t_high_ld);
  std::uniform_int_distribution<int64_t> dist(t_low, t_high);
  int64_t t = dist(rng);

  for (size_t i = 0; i < point.size(); ++i) {
    __int128 next = static_cast<__int128>(point[i]) +
                    static_cast<__int128>(t) * direction[i];
    if (next < std::numeric_limits<int64_t>::min() ||
        next > std::numeric_limits<int64_t>::max()) {
      return false;
    }
    point[i] = static_cast<int64_t>(next);
  }

  return true;
}

} // namespace RegionSampling
