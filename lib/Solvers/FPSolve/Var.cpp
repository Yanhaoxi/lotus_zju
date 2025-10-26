/**
 * Variable management implementation for FPSolve
 */

#include "Solvers/FPSolve/DataStructs/Var.h"

namespace fpsolve {

// Static member initialization
VarId Var::next_id_{0};
std::unordered_map<std::string, VarId> Var::name_to_id_;
std::unordered_map<VarId, std::unique_ptr<Var>> Var::id_to_var_;

std::ostream& operator<<(std::ostream &out, const VarId &vid) {
  return out << Var::GetVar(vid).string();
}

std::ostream& operator<<(std::ostream &out, const std::vector<VarId> vids) {
  out << "[";
  bool first = true;
  for (const auto &vid : vids) {
    if (!first) {
      out << ", ";
    }
    first = false;
    out << Var::GetVar(vid).string();
  }
  out << "]";
  return out;
}

} // namespace fpsolve

