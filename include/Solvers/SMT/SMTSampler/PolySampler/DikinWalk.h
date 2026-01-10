#pragma once

#include <cstdint>
#include <random>
#include <vector>

#include "Solvers/SMT/SMTSampler/PolySampler/RegionSamplingTypes.h"

namespace RegionSampling {

bool dikin_walk_step(const std::vector<LinearConstraint> &constraints,
                     std::vector<int64_t> &point,
                     std::mt19937_64 &rng);

} // namespace RegionSampling
