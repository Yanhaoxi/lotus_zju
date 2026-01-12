#pragma once

#include "Alias/TPA/Context/Context.h"
#include "Alias/TPA/Context/ProgramPoint.h"

#include <unordered_set>

namespace context
{

class AdaptiveContext
{
private:
	static std::unordered_set<ProgramPoint> trackedCallsites;
public:
	static void trackCallSite(const ProgramPoint&);

	static const Context* pushContext(const Context*, const llvm::Instruction*);
	static const Context* pushContext(const ProgramPoint&);
};

} // namespace context
