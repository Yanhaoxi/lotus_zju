#include "Alias/TPA/PointerAnalysis/Analysis/GlobalPointerAnalysis.h"
#include "Alias/TPA/PointerAnalysis/Analysis/SemiSparsePointerAnalysis.h"
#include "Alias/TPA/PointerAnalysis/Engine/GlobalState.h"
#include "Alias/TPA/PointerAnalysis/Engine/Initializer.h"
#include "Alias/TPA/PointerAnalysis/Engine/SemiSparsePropagator.h"
#include "Alias/TPA/PointerAnalysis/Engine/TransferFunction.h"
#include "Alias/TPA/PointerAnalysis/Program/SemiSparseProgram.h"
#include "Alias/TPA/Util/AnalysisEngine/DataFlowAnalysis.h"

namespace tpa
{

void SemiSparsePointerAnalysis::runOnProgram(const SemiSparseProgram& ssProg)
{
	auto initStore = Store();
	std::tie(env, initStore) = GlobalPointerAnalysis(ptrManager, memManager, ssProg.getTypeMap()).runOnModule(ssProg.getModule());

	auto globalState = GlobalState(ptrManager, memManager, ssProg, extTable, env);
	auto dfa = util::DataFlowAnalysis<GlobalState, Memo, TransferFunction, SemiSparsePropagator>(globalState, memo);
	dfa.runOnInitialState<Initializer>(std::move(initStore));
}

PtsSet SemiSparsePointerAnalysis::getPtsSetImpl(const Pointer* ptr) const
{
	return env.lookup(ptr);
}

} // namespace tpa