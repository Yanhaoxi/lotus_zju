/**
 * Variable Degree Map for polynomials
 */

#ifndef FPSOLVE_VAR_DEGREE_MAP_H
#define FPSOLVE_VAR_DEGREE_MAP_H

#include "Solvers/FPSolve/DataStructs/Var.h"
#include <algorithm>
#include <map>

namespace fpsolve {

class VarDegreeMap {
public:
  VarDegreeMap() = default;
  
  void Merge(const VarDegreeMap &other) {
    for (const auto &var_degree : other.map_) {
      auto iter = map_.find(var_degree.first);
      if (iter == map_.end()) {
        map_.insert(var_degree);
      } else {
        iter->second = std::max(iter->second, var_degree.second);
      }
    }
  }

  void Insert(VarId var, Degree degree) {
    auto iter = map_.find(var);
    if (iter == map_.end()) {
      map_.insert(std::make_pair(var, degree));
    } else {
      iter->second = std::max(iter->second, degree);
    }
  }

  Degree GetDegree(VarId var) const {
    auto iter = map_.find(var);
    if (iter == map_.end()) {
      return 0;
    }
    return iter->second;
  }

  bool empty() const { return map_.empty(); }

  bool operator==(const VarDegreeMap &other) const {
    return map_ == other.map_;
  }

  auto begin() const { return map_.begin(); }
  auto end() const { return map_.end(); }

private:
  std::map<VarId, Degree> map_;
};

} // namespace fpsolve

#endif // FPSOLVE_VAR_DEGREE_MAP_H

