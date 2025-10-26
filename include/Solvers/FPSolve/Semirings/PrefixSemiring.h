#ifndef FPSOLVE_PREFIX_SEMIRING_H
#define FPSOLVE_PREFIX_SEMIRING_H

#include <string>
#include <vector>
#include <set>
#include "Solvers/FPSolve/Semirings/Semiring.h"
#include "Solvers/FPSolve/DataStructs/Var.h"

namespace fpsolve {

class PrefixSemiring : public StarableSemiring<PrefixSemiring, Commutativity::NonCommutative, Idempotence::Idempotent>
{
private:
	std::set<std::vector<VarId>> val;
	unsigned int max_length;
	static std::vector<VarId> concatenate(std::vector<VarId> l, std::vector<VarId> r, unsigned int length);
	static std::shared_ptr<PrefixSemiring> elem_null;
	static std::shared_ptr<PrefixSemiring> elem_one;
public:
	PrefixSemiring();
	PrefixSemiring(const std::vector<VarId>& val, unsigned int length);
	virtual ~PrefixSemiring();
	PrefixSemiring operator += (const PrefixSemiring& elem);
	PrefixSemiring operator *= (const PrefixSemiring& elem);
	bool operator == (const PrefixSemiring& elem) const;
	PrefixSemiring star () const;
	static PrefixSemiring null();
	static PrefixSemiring one();
	std::string string() const;
};

} // namespace fpsolve

#endif // FPSOLVE_PREFIX_SEMIRING_H

