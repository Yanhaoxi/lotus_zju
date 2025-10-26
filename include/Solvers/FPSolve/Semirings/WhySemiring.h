#ifndef FPSOLVE_WHY_SEMIRING_H
#define FPSOLVE_WHY_SEMIRING_H

#include "Solvers/FPSolve/Semirings/Semiring.h"
#include "Solvers/FPSolve/DataStructs/Var.h"
#include <set>

namespace fpsolve {

typedef std::set<VarId> VarSet;
typedef std::set<VarSet> WhySet;

class WhySemiring : public StarableSemiring<WhySemiring, Commutativity::Commutative, Idempotence::Idempotent>{

private:
  WhySet val;
  static std::shared_ptr<WhySemiring> elem_null;
  static std::shared_ptr<WhySemiring> elem_one;
public:
  WhySemiring();
  WhySemiring(VarId v);
  WhySemiring(std::string str_val);
  virtual ~WhySemiring();
  WhySemiring operator += (const WhySemiring& elem);
  WhySemiring operator *= (const WhySemiring& elem);
  bool operator == (const WhySemiring& elem) const;
  WhySemiring star () const;
  static WhySemiring null();
  static WhySemiring one();
  VarSet getVars() const;
  std::string string() const;
};

} // namespace fpsolve

#endif // FPSOLVE_WHY_SEMIRING_H

