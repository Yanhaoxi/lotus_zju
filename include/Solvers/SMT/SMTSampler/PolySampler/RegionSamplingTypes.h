#pragma once

#include <cstdint>
#include <vector>

namespace RegionSampling {

struct LinearConstraint {
  std::vector<int64_t> coeffs;
  int64_t bound;
};

} // namespace RegionSampling
