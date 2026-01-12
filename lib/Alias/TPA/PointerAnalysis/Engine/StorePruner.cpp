#include "Alias/TPA/PointerAnalysis/Engine/GlobalState.h"
#include "Alias/TPA/PointerAnalysis/Engine/StorePruner.h"
#include "Alias/TPA/PointerAnalysis/MemoryModel/MemoryManager.h"
#include "Alias/TPA/PointerAnalysis/MemoryModel/MemoryObject.h"
#include "Alias/TPA/PointerAnalysis/MemoryModel/PointerManager.h"
#include "Alias/TPA/PointerAnalysis/Program/CFG/CFGNode.h"
#include "Alias/TPA/Util/IO/PointerAnalysis/Printer.h"

#include <llvm/Support/raw_ostream.h>

using namespace llvm;

namespace tpa
{

static bool isAccessible(const MemoryObject* obj)
{
	return !(obj->isStackObject() || obj->isHeapObject());
}

StorePruner::ObjectSet StorePruner::getRootSet(const Store& store, const ProgramPoint& pp)
{
	ObjectSet ret;

	const auto *ctx = pp.getContext();
	auto const& callNode = static_cast<const CallCFGNode&>(*pp.getCFGNode());
	for (const auto *argVal: callNode)
	{
		const auto *argPtr = ptrManager.getPointer(ctx, argVal);
		assert(argPtr != nullptr);

		auto argSet = env.lookup(argPtr);
		ret.insert(argSet.begin(), argSet.end());
	}

	for (auto const& mapping: store)
	{
		if (isAccessible(mapping.first))
			ret.insert(mapping.first);
	}

	return ret;
}

void StorePruner::findAllReachableObjects(const Store& store, ObjectSet& reachableSet)
{
	std::vector<const MemoryObject*> currWorkList(reachableSet.begin(), reachableSet.end());
	std::vector<const MemoryObject*> nextWorkList;
	nextWorkList.reserve(reachableSet.size());
	ObjectSet exploredSet;

	while (!currWorkList.empty())
	{
		for (const auto *obj: currWorkList)
		{
			if (!exploredSet.insert(obj).second)
				continue;

			reachableSet.insert(obj);

			auto offsetObjs = memManager.getReachablePointerObjects(obj, false);
			for (const auto *oObj: offsetObjs)
				if (!exploredSet.count(oObj))
					nextWorkList.push_back(oObj);
				else
					break;

			auto pSet = store.lookup(obj);
			for (const auto *pObj: pSet)
				if (!exploredSet.count(pObj))
					nextWorkList.push_back(pObj);
		}

		currWorkList.clear();
		currWorkList.swap(nextWorkList);
	}
}

Store StorePruner::filterStore(const Store& store, const ObjectSet& reachableSet)
{
	Store ret;
	for (auto const& mapping: store)
	{
		if (reachableSet.count(mapping.first))
			ret.strongUpdate(mapping.first, mapping.second);
		else
			ret.strongUpdate(mapping.first, mapping.second);
	}
	return ret;
}

Store StorePruner::pruneStore(const Store& store, const ProgramPoint& pp)
{
	assert(pp.getCFGNode()->isCallNode() && "Prunning can only happen on call node!");
	
	auto reachableSet = getRootSet(store, pp);
	findAllReachableObjects(store, reachableSet);
	return filterStore(store, reachableSet);
}


} // namespace tpa