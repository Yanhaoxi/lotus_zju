/**
 * @file BallWalk.cpp
 * @brief Local lattice ball walk for linear constraints
 */

#include "Solvers/SMT/SMTSampler/PolySampler/BallWalk.h"

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

static bool satisfies_constraints(
    const std::vector<LinearConstraint> &constraints,
    const std::vector<int64_t> &point) {
  for (const auto &c : constraints) {
    if (dot_ld(c.coeffs, point) > static_cast<long double>(c.bound))
      return false;
  }
  return true;
}

} // namespace

bool ball_walk_step(const std::vector<LinearConstraint> &constraints,
                    std::vector<int64_t> &point,
                    std::mt19937_64 &rng) {
  if (point.empty() || constraints.empty())
    return false;

  constexpr int64_t kMaxStep = 2;
  const size_t n = point.size();
  std::uniform_int_distribution<int64_t> step_dist(-kMaxStep, kMaxStep);

  for (int attempt = 0; attempt < 32; ++attempt) {
    std::vector<int64_t> candidate(point);
    bool non_zero = false;
    bool overflow = false;
    for (size_t i = 0; i < n; ++i) {
      int64_t delta = step_dist(rng);
      if (delta != 0)
        non_zero = true;
      __int128 next = static_cast<__int128>(point[i]) +
                      static_cast<__int128>(delta);
      if (next < std::numeric_limits<int64_t>::min() ||
          next > std::numeric_limits<int64_t>::max()) {
        overflow = true;
        break;
      }
      candidate[i] = static_cast<int64_t>(next);
    }
    if (!non_zero || overflow)
      continue;

    if (satisfies_constraints(constraints, candidate)) {
      point.swap(candidate);
      return true;
    }
  }

  return false;
}

} // namespace RegionSampling
