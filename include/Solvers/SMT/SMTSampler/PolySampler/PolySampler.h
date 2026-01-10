#pragma once

#include <cstdint>
#include <functional>
#include <random>
#include <vector>

#include "Solvers/SMT/SMTSampler/PolySampler/RegionSamplingTypes.h"

namespace RegionSampling {

enum class Walk {
  HitAndRun,
  Dikin,
  Coordinate
};

struct SampleConfig {
  int max_samples = 1000;
  double max_time_ms = 30000.0;
  int burn_in_steps = 20;
  int steps_per_sample = 5;
  int max_attempts_factor = 100;
};

std::vector<std::vector<int64_t>>
sample_points(const std::vector<LinearConstraint> &constraints,
              std::vector<int64_t> point,
              Walk walk,
              std::mt19937_64 &rng,
              const SampleConfig &config,
              const std::function<bool(const std::vector<int64_t> &)> &accept);

} // namespace RegionSampling
