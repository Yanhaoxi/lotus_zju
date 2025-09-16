#pragma once
#include "Alias/OriginAA/KCallsitePointerAnalysis.h"
#include "Alias/OriginAA/CallGraph.h"
#include <fstream>
#include <deque>


namespace llvm
{

    class OriginPointerAnalysis : public KCallsitePointerAnalysis
    {
    public:
        OriginPointerAnalysis(unsigned k, Module &M) : KCallsitePointerAnalysis(k, M) {}

        // Override getContext to only use thread creation and tokio::task::spawn as context
        Context getContext(Context context, const Value *newCallSite) override;
    };

} // namespace llvm