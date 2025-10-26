/**
 * Commutative monomial implementation for FPSolve
 */

#include "Solvers/FPSolve/Polynomials/CommutativeMonomial.h"
#include <iostream>

namespace fpsolve {

std::ostream& operator<<(std::ostream &out, const CommutativeMonomial &monomial) {
  return out << monomial.string();
}

} // namespace fpsolve

