/**
 * @file CoordinateWalk.cpp
 * @brief Axis-aligned walk for linear constraints
 */

#include "Solvers/SMT/SMTSampler/PolySampler/CoordinateWalk.h"

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

bool coordinate_walk_step(const std::vector<LinearConstraint> &constraints,
                          std::vector<int64_t> &point,
                          std::mt19937_64 &rng) {
  if (point.empty())
    return false;

  const size_t n = point.size();
  std::uniform_int_distribution<size_t> axis_dist(0, n - 1);
  std::uniform_int_distribution<int> sign_dist(0, 1);
  size_t axis = axis_dist(rng);
  int sign = sign_dist(rng) ? 1 : -1;

  for (int attempt = 0; attempt < 2; ++attempt) {
    long double t_min = -std::numeric_limits<long double>::infinity();
    long double t_max = std::numeric_limits<long double>::infinity();
    std::vector<int64_t> direction(n, 0);
    direction[axis] = sign;

    for (const auto &c : constraints) {
      long double a_dot_x = dot_ld(c.coeffs, point);
      long double a_dot_d =
          static_cast<long double>(c.coeffs[axis]) * sign;
      long double slack = static_cast<long double>(c.bound) - a_dot_x;

      if (a_dot_d > 0.0L) {
        t_max = std::min(t_max, slack / a_dot_d);
      } else if (a_dot_d < 0.0L) {
        t_min = std::max(t_min, slack / a_dot_d);
      } else if (slack < 0.0L) {
        return false;
      }
    }

    if (!std::isfinite(t_min) || !std::isfinite(t_max) || t_min > t_max) {
      sign *= -1;
      continue;
    }

    long double t_low_ld = std::ceil(t_min);
    long double t_high_ld = std::floor(t_max);
    if (t_low_ld > t_high_ld) {
      sign *= -1;
      continue;
    }

    int64_t t_low = static_cast<int64_t>(t_low_ld);
    int64_t t_high = static_cast<int64_t>(t_high_ld);
    if (t_low == 0 && t_high == 0) {
      sign *= -1;
      continue;
    }

    std::uniform_int_distribution<int64_t> dist(t_low, t_high);
    int64_t t = 0;
    for (int tries = 0; tries < 4; ++tries) {
      t = dist(rng);
      if (t != 0)
        break;
    }
    if (t == 0) {
      sign *= -1;
      continue;
    }

    __int128 next = static_cast<__int128>(point[axis]) +
                    static_cast<__int128>(t) * sign;
    if (next < std::numeric_limits<int64_t>::min() ||
        next > std::numeric_limits<int64_t>::max()) {
      sign *= -1;
      continue;
    }
    point[axis] = static_cast<int64_t>(next);
    return true;
  }

  return false;
}

} // namespace RegionSampling
