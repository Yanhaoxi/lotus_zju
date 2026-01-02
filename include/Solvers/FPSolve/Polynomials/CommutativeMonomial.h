/**
 * Commutative Monomial for FPSolve
 */

#ifndef FPSOLVE_COMMUTATIVE_MONOMIAL_H
#define FPSOLVE_COMMUTATIVE_MONOMIAL_H

#include "Solvers/FPSolve/DataStructs/Var.h"
#include "Solvers/FPSolve/DataStructs/VarDegreeMap.h"
#include <initializer_list>
#include <map>
#include <sstream>
#include <string>

namespace fpsolve {

class CommutativeMonomial {
public:
  CommutativeMonomial() = default;

  CommutativeMonomial(std::initializer_list<VarId> vars) {
    for (const auto &var : vars) {
      variables_.Insert(var, variables_.GetDegree(var) + 1);
    }
  }

  CommutativeMonomial(const std::map<VarId, Degree> &vars) {
    for (const auto &var_degree : vars) {
      variables_.Insert(var_degree.first, var_degree.second);
    }
  }

  CommutativeMonomial operator*(const CommutativeMonomial &other) const {
    CommutativeMonomial result = *this;
    result.variables_.Merge(other.variables_);
    return result;
  }

  bool operator<(const CommutativeMonomial &other) const {
    return string() < other.string();
  }

  bool operator==(const CommutativeMonomial &other) const {
    return variables_ == other.variables_;
  }

  Degree get_degree() const {
    Degree total = 0;
    for (const auto &var_degree : variables_) {
      total += var_degree.second;
    }
    return total;
  }

  std::string string() const {
    if (variables_.empty()) {
      return "1";
    }
    std::stringstream ss;
    bool first = true;
    for (const auto &var_degree : variables_) {
      if (!first) {
        ss << "*";
      }
      first = false;
      ss << Var::GetVar(var_degree.first).string();
      if (var_degree.second > 1) {
        ss << "^" << var_degree.second;
      }
    }
    return ss.str();
  }

  VarDegreeMap variables_;
};

} // namespace fpsolve

namespace std {
template <>
struct hash<fpsolve::CommutativeMonomial> {
  std::size_t operator()(const fpsolve::CommutativeMonomial &m) const {
    return std::hash<std::string>()(m.string());
  }
};
} // namespace std

#endif // FPSOLVE_COMMUTATIVE_MONOMIAL_H

