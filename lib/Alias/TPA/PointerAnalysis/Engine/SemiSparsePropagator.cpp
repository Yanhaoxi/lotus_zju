#include "Alias/TPA/PointerAnalysis/Engine/EvalResult.h"
#include "Alias/TPA/PointerAnalysis/Engine/GlobalState.h"
#include "Alias/TPA/PointerAnalysis/Engine/SemiSparsePropagator.h"
#include "Alias/TPA/PointerAnalysis/Engine/WorkList.h"
#include "Alias/TPA/PointerAnalysis/Program/CFG/CFG.h"
#include "Alias/TPA/PointerAnalysis/Support/Memo.h"

#include "Alias/TPA/Util/IO/PointerAnalysis/Printer.h"
#include <llvm/Support/raw_ostream.h>
using namespace llvm;

namespace tpa
{

namespace
{

bool isTopLevelNode(const CFGNode* node)
{
	return node->isAllocNode() || node->isCopyNode() || node->isOffsetNode();
}

} // namespace

bool SemiSparsePropagator::enqueueIfMemoChange(const ProgramPoint& pp, const Store& store)
{
	if (memo.update(pp, store))
	{
		workList.enqueue(pp);
		return true;
	}
	else
		return false;
}

void SemiSparsePropagator::propagateTopLevel(const EvalSuccessor& evalSucc)
{
	// Top-level successors: no store merging, just enqueue
	workList.enqueue(evalSucc.getProgramPoint());
	//errs() << "\tENQ(T) " << evalSucc.getProgramPoint() << "\n";
}

void SemiSparsePropagator::propagateMemLevel(const EvalSuccessor& evalSucc)
{
	// Mem-level successors: store merging, enqueue if memo changed
	const auto *node = evalSucc.getProgramPoint().getCFGNode();
	assert(!isTopLevelNode(node));
	assert(evalSucc.getStore() != nullptr);
	bool enqueued = enqueueIfMemoChange(evalSucc.getProgramPoint(), *evalSucc.getStore());

	//if (enqueued)
	//	errs() << "\tENQ(M) " << evalSucc.getProgramPoint() << "\n";
}

void SemiSparsePropagator::propagate(const EvalResult& evalResult)
{
	for (auto const& evalSucc: evalResult)
	{
		if (evalSucc.isTopLevel())
			propagateTopLevel(evalSucc);
		else
			propagateMemLevel(evalSucc);
	}
}

} // namespace tpa
