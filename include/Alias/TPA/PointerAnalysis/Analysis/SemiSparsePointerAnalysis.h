#pragma once

#include "Alias/TPA/PointerAnalysis/Analysis/PointerAnalysis.h"
#include "Alias/TPA/PointerAnalysis/Support/Env.h"
#include "Alias/TPA/PointerAnalysis/Support/Memo.h"

namespace tpa
{

class SemiSparseProgram;

class SemiSparsePointerAnalysis: public PointerAnalysis<SemiSparsePointerAnalysis>
{
private:
	Env env;
	Memo memo;
public:
	SemiSparsePointerAnalysis() = default;

	void runOnProgram(const SemiSparseProgram&);

	PtsSet getPtsSetImpl(const Pointer*) const;
};

} // namespace tpa
