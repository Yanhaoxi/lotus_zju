/**
 * Tropical Semiring implementation
 */

#include "Solvers/FPSolve/Semirings/TropicalSemiring.h"

namespace fpsolve {

TropicalSemiring::TropicalSemiring(std::string str_val) {
  if (str_val == "inf" || str_val == "infinity") {
    val = INFTY;
  } else {
    val = std::stoi(str_val);
  }
}

} // namespace fpsolve

