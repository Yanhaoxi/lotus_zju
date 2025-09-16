#include "Alias/OriginAA/KCallsitePointerAnalysis.h"
#include "Alias/OriginAA/Flags.h"
#include <utility>

using namespace llvm;

// Create a new context by appending the new call site, keeping only the most recent K call sites
Context KCallsitePointerAnalysis::getContext(Context context, const Value *newCallSite)
{
    Context newContext = context;
    newContext.values.push_back(newCallSite);
    if (newContext.values.size() > K)
    {
        newContext.values.pop_front();
    }

    if (DebugMode)
        errs() << "New context for call site: " << newContext << "\n";

    return newContext;
}