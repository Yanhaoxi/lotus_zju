/**
 * Commutative Polynomial for FPSolve
 */

#ifndef FPSOLVE_COMMUTATIVE_POLYNOMIAL_H
#define FPSOLVE_COMMUTATIVE_POLYNOMIAL_H

#include "Solvers/FPSolve/DataStructs/Var.h"
#include "Solvers/FPSolve/DataStructs/Matrix.h"
#include "Solvers/FPSolve/Polynomials/CommutativeMonomial.h"
#include "Solvers/FPSolve/Semirings/Semiring.h"
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace fpsolve {

template <typename SR> 
using MonomialMap = std::map<CommutativeMonomial, SR>;

template <typename SR>
class CommutativePolynomial : public Semiring<CommutativePolynomial<SR>,
                                   Commutativity::Commutative,
                                   SR::GetIdempotence()> {
private:
  MonomialMap<SR> monomials_;
  VarDegreeMap variables_;

  void InsertMonomial(const CommutativeMonomial &m, const SR &c) {
    if (c == SR::null()) {
      return;
    }
    monomials_.insert(std::make_pair(m, c));
  }

  CommutativePolynomial(MonomialMap<SR> &&ms, VarDegreeMap &&vs)
      : monomials_(std::move(ms)), variables_(std::move(vs)) {}

public:
  CommutativePolynomial() {
    monomials_[CommutativeMonomial()] = SR::null();
  }

  CommutativePolynomial(const SR &c) {
    monomials_[CommutativeMonomial()] = c;
  }

  CommutativePolynomial(std::initializer_list<std::pair<SR, CommutativeMonomial>> terms) {
    for (const auto &term : terms) {
      InsertMonomial(term.second, term.first);
      variables_.Merge(term.second.variables_);
    }
    if (monomials_.empty()) {
      monomials_[CommutativeMonomial()] = SR::null();
    }
  }

  // Constructor from VarId - creates polynomial with single variable
  explicit CommutativePolynomial(VarId var) {
    std::map<VarId, Degree> var_map;
    var_map[var] = 1;
    CommutativeMonomial monomial(var_map);
    monomials_[monomial] = SR::one();
    variables_.Insert(var, 1);
  }

  static CommutativePolynomial null() {
    return CommutativePolynomial(SR::null());
  }

  static CommutativePolynomial one() {
    return CommutativePolynomial(SR::one());
  }

  CommutativePolynomial operator+=(const CommutativePolynomial &other) {
    for (const auto &monomial_coeff : other.monomials_) {
      auto iter = monomials_.find(monomial_coeff.first);
      if (iter == monomials_.end()) {
        InsertMonomial(monomial_coeff.first, monomial_coeff.second);
      } else {
        SR new_coeff = iter->second + monomial_coeff.second;
        if (new_coeff == SR::null()) {
          monomials_.erase(iter);
        } else {
          iter->second = new_coeff;
        }
      }
    }
    variables_.Merge(other.variables_);
    if (monomials_.empty()) {
      monomials_[CommutativeMonomial()] = SR::null();
    }
    return *this;
  }

  CommutativePolynomial operator*=(const CommutativePolynomial &other) {
    if (monomials_.size() == 1 && 
        monomials_.begin()->first == CommutativeMonomial() &&
        monomials_.begin()->second == SR::null()) {
      return *this;
    }
    if (other.monomials_.size() == 1 && 
        other.monomials_.begin()->first == CommutativeMonomial() &&
        other.monomials_.begin()->second == SR::null()) {
      *this = other;
      return *this;
    }

    MonomialMap<SR> new_monomials;
    VarDegreeMap new_variables;

    for (const auto &m1 : monomials_) {
      for (const auto &m2 : other.monomials_) {
        CommutativeMonomial new_mon = m1.first * m2.first;
        SR new_coeff = m1.second * m2.second;
        
        auto iter = new_monomials.find(new_mon);
        if (iter == new_monomials.end()) {
          if (new_coeff != SR::null()) {
            new_monomials[new_mon] = new_coeff;
            new_variables.Merge(new_mon.variables_);
          }
        } else {
          iter->second += new_coeff;
          if (iter->second == SR::null()) {
            new_monomials.erase(iter);
          }
        }
      }
    }

    if (new_monomials.empty()) {
      new_monomials[CommutativeMonomial()] = SR::null();
    }

    monomials_ = std::move(new_monomials);
    variables_ = std::move(new_variables);
    return *this;
  }

  bool operator==(const CommutativePolynomial &other) const override {
    return monomials_ == other.monomials_;
  }

  // Multiply polynomial by a variable
  CommutativePolynomial operator*(VarId var) const {
    return *this * CommutativePolynomial(var);
  }

  std::string string() const override {
    if (monomials_.empty()) {
      return "0";
    }
    std::stringstream ss;
    bool first = true;
    for (const auto &term : monomials_) {
      if (!first) {
        ss << " + ";
      }
      first = false;
      ss << "(" << term.second.string() << ")*" << term.first.string();
    }
    return ss.str();
  }

  Degree get_degree() const {
    Degree max_degree = 0;
    for (const auto &term : monomials_) {
      max_degree = std::max(max_degree, term.first.get_degree());
    }
    return max_degree;
  }

  const VarDegreeMap& GetVarDegreeMap() const {
    return variables_;
  }

  std::vector<VarId> get_variables() const {
    std::vector<VarId> vars;
    for (const auto &var_degree : variables_) {
      vars.push_back(var_degree.first);
    }
    return vars;
  }

  SR eval(const ValuationMap<SR> &valuation) const {
    SR result = SR::null();
    for (const auto &term : monomials_) {
      SR monomial_val = term.second;
      for (const auto &var_degree : term.first.variables_) {
        auto iter = valuation.find(var_degree.first);
        if (iter != valuation.end()) {
          SR var_val = iter->second;
          for (Degree i = 0; i < var_degree.second; ++i) {
            monomial_val *= var_val;
          }
        } else {
          monomial_val = SR::null();
          break;
        }
      }
      result += monomial_val;
    }
    return result;
  }

  CommutativePolynomial partial_eval(const ValuationMap<SR> &valuation) const {
    return *this; // Simplified version
  }

  template <typename F>
  auto Map(F fun) const 
      -> CommutativePolynomial<typename std::result_of<F(SR)>::type> {
    using NewSR = typename std::result_of<F(SR)>::type;
    MonomialMap<NewSR> new_monomials;
    
    for (const auto &term : monomials_) {
      NewSR new_coeff = fun(term.second);
      if (new_coeff != NewSR::null()) {
        new_monomials[term.first] = new_coeff;
      }
    }

    if (new_monomials.empty()) {
      new_monomials[CommutativeMonomial()] = NewSR::null();
    }

    return CommutativePolynomial<NewSR>(std::move(new_monomials), VarDegreeMap(variables_));
  }

  static Matrix<CommutativePolynomial<SR>> jacobian(
      const std::vector<CommutativePolynomial<SR>> &polynomials,
      const std::vector<VarId> &variables);
};

} // namespace fpsolve

#endif // FPSOLVE_COMMUTATIVE_POLYNOMIAL_H

