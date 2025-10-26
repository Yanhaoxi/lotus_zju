/**
 * Boolean Semiring implementation
 */

#include "Solvers/FPSolve/Semirings/BoolSemiring.h"

namespace fpsolve {

std::shared_ptr<BoolSemiring> BoolSemiring::elem_null = nullptr;
std::shared_ptr<BoolSemiring> BoolSemiring::elem_one = nullptr;

BoolSemiring::BoolSemiring() : val(false) {}

BoolSemiring::BoolSemiring(bool val) : val(val) {}

BoolSemiring::BoolSemiring(std::string str_val) {
  if (str_val == "true" || str_val == "1") {
    val = true;
  } else {
    val = false;
  }
}

BoolSemiring::~BoolSemiring() {}

BoolSemiring BoolSemiring::operator+=(const BoolSemiring& elem) {
  val = val || elem.val;
  return *this;
}

BoolSemiring BoolSemiring::operator*=(const BoolSemiring& elem) {
  val = val && elem.val;
  return *this;
}

bool BoolSemiring::operator==(const BoolSemiring& elem) const {
  return val == elem.val;
}

BoolSemiring BoolSemiring::star() const {
  return BoolSemiring::one();
}

BoolSemiring BoolSemiring::null() {
  return BoolSemiring(false);
}

BoolSemiring BoolSemiring::one() {
  return BoolSemiring(true);
}

std::string BoolSemiring::string() const {
  return val ? "true" : "false";
}

} // namespace fpsolve

