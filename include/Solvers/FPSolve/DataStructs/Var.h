/**
 * Variable management for FPSolve
 */

#ifndef FPSOLVE_VAR_H
#define FPSOLVE_VAR_H

#include <cassert>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace fpsolve {

class Var;

class VarId {
public:
  VarId() : id_(std::numeric_limits<std::uint_fast32_t>::max()) {}
  VarId(std::uint_fast32_t i) : id_(i) {}

  VarId(const VarId &v) = default;
  VarId(VarId &&v) = default;

  VarId& operator=(const VarId &v) = default;
  VarId& operator=(VarId &&v) = default;

  inline bool operator==(const VarId rhs) const { return id_ == rhs.id_; }
  inline bool operator!=(const VarId rhs) const { return id_ != rhs.id_; }
  inline bool operator<(const VarId rhs) const { return id_ < rhs.id_; }
  inline bool operator>(const VarId rhs) const { return id_ > rhs.id_; }

  VarId operator++() { ++id_; return *this; }

  inline std::size_t Hash() const {
    std::hash<std::uint_fast32_t> h;
    return h(id_);
  }

  std::uint_fast32_t GetRawId() const { return id_; }

private:
  std::uint_fast32_t id_;
};

std::ostream& operator<<(std::ostream &out, const VarId &vid);

} // namespace fpsolve

namespace std {
template <>
struct hash<fpsolve::VarId> {
  inline std::size_t operator()(const fpsolve::VarId vid) const {
    return vid.Hash();
  }
};
} // namespace std

namespace fpsolve {

class Var {
public:
  static VarId GetVarId() {
    std::stringstream ss;
    ss << "_" << next_id_.GetRawId();
    return GetVarId(ss.str());
  }

  static VarId GetVarId(const std::string &name) {
    auto iter = name_to_id_.find(name);
    if (iter != name_to_id_.end()) {
      return iter->second;
    }

    std::unique_ptr<Var> var{new Var(next_id_, name)};
    ++next_id_;
    auto iter_inserted = id_to_var_.emplace(var->GetId(), std::move(var));
    assert(iter_inserted.second);
    auto &inserted_var = iter_inserted.first->second;
    name_to_id_.emplace(inserted_var->GetName(), inserted_var->GetId());
    return inserted_var->GetId();
  }

  static const Var& GetVar(const VarId vid) {
    auto iter = id_to_var_.find(vid);
    assert(iter != id_to_var_.end());
    return *iter->second;
  }

  inline bool operator==(const Var &rhs) const {
    return id_ == rhs.id_;
  }

  inline bool operator<(const Var &rhs) const {
    return id_ < rhs.id_;
  }

  inline VarId GetId() const { return id_; }

  inline std::string string() const { return name_; }

  friend std::ostream& operator<<(std::ostream &out, const Var &var) {
    return out << var.string();
  }

private:
  Var(const VarId i, const std::string &n) : id_(i), name_(n) {}
  Var(const VarId i, std::string &&n) : id_(i), name_(std::move(n)) {}

  VarId id_;
  std::string name_;

  static VarId next_id_;
  static std::unordered_map<std::string, VarId> name_to_id_;
  static std::unordered_map<VarId, std::unique_ptr<Var>> id_to_var_;

  std::string GetName() const { return name_; }
};

std::ostream& operator<<(std::ostream &out, const std::vector<VarId> vids);

template <typename SR> 
using ValuationMap = std::unordered_map<VarId, SR>;

using SubstitutionMap = std::unordered_map<VarId, VarId>;

using Degree = std::uint_fast16_t;

} // namespace fpsolve

#endif // FPSOLVE_VAR_H

