/**
 * @file PolySampler.cpp
 * @brief Sampling loop for polytopes with pluggable walk strategies
 */

#include "Solvers/SMT/SMTSampler/PolySampler/PolySampler.h"

#include <chrono>
#include <sstream>
#include <unordered_set>

#include "Solvers/SMT/SMTSampler/PolySampler/BallWalk.h"
#include "Solvers/SMT/SMTSampler/PolySampler/ConstraintWalk.h"
#include "Solvers/SMT/SMTSampler/PolySampler/CoordinateWalk.h"
#include "Solvers/SMT/SMTSampler/PolySampler/DikinWalk.h"
#include "Solvers/SMT/SMTSampler/PolySampler/HitAndRun.h"

namespace RegionSampling {
namespace {

static std::string point_key(const std::vector<int64_t> &point) {
  std::ostringstream oss;
  for (size_t i = 0; i < point.size(); ++i) {
    if (i)
      oss << ",";
    oss << point[i];
  }
  return oss.str();
}

static bool walk_step(const std::vector<LinearConstraint> &constraints,
                      std::vector<int64_t> &point,
                      Walk walk,
                      std::mt19937_64 &rng) {
  switch (walk) {
  case Walk::HitAndRun:
    return RegionSampling::hit_and_run_step(constraints, point, rng);
  case Walk::Dikin:
    return RegionSampling::dikin_walk_step(constraints, point, rng);
  case Walk::Coordinate:
    return RegionSampling::coordinate_walk_step(constraints, point, rng);
  case Walk::Constraint:
    return RegionSampling::constraint_walk_step(constraints, point, rng);
  case Walk::Ball:
    return RegionSampling::ball_walk_step(constraints, point, rng);
  }
  return RegionSampling::coordinate_walk_step(constraints, point, rng);
}

} // namespace

std::vector<std::vector<int64_t>>
sample_points(const std::vector<LinearConstraint> &constraints,
              std::vector<int64_t> point,
              Walk walk,
              std::mt19937_64 &rng,
              const SampleConfig &config,
              const std::function<bool(const std::vector<int64_t> &)> &accept) {
  std::vector<std::vector<int64_t>> samples;
  if (constraints.empty() || point.empty())
    return samples;

  for (int i = 0; i < config.burn_in_steps; ++i) {
    walk_step(constraints, point, walk, rng);
  }

  std::unordered_set<std::string> seen;
  auto start = std::chrono::high_resolution_clock::now();
  int attempts = 0;

  while (static_cast<int>(samples.size()) < config.max_samples) {
    auto now = std::chrono::high_resolution_clock::now();
    double elapsed_ms =
        std::chrono::duration<double, std::milli>(now - start).count();
    if (elapsed_ms > config.max_time_ms) {
      break;
    }

    for (int step = 0; step < config.steps_per_sample; ++step) {
      if (!walk_step(constraints, point, walk, rng))
        break;
    }

    std::string key = point_key(point);
    if (seen.find(key) != seen.end()) {
      attempts++;
      continue;
    }
    seen.insert(key);

    if (!accept || accept(point)) {
      samples.push_back(point);
    }

    attempts++;
    if (attempts > config.max_samples * config.max_attempts_factor) {
      break;
    }
  }

  return samples;
}

} // namespace RegionSampling
