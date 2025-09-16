#pragma once
#include "llvm/IR/Value.h"
#include "Alias/OriginAA/PointerAnalysis.h"
#include "Alias/OriginAA/CallGraph.h"
#include <deque>

namespace llvm
{

    class KCallsitePointerAnalysis : public PointerAnalysis
    {
    public:
        unsigned K = 1; // Number of call sites to track for each function

        KCallsitePointerAnalysis(unsigned k, Module &M) : PointerAnalysis(M), K(k) {}

        // context related methods: context is from caller, value is the new callsite, return a new context
        Context getContext(Context context, const Value *newCallSite) override;

        // Override processInstruction to propagate call string
        void processInstruction(Instruction &I, CGNode *cgnode) override
        {
            CurrentCGNode = cgnode;
            CurrentContext = getContext(cgnode->context, &I);
            visit(I); // Will use base class visit* unless overridden here
        }

        // // Override call instruction handlers to propagate call string
        // void visitInvokeInst(InvokeInst &II) override;
        // void visitCallInst(CallInst &CI) override;
    };

} // namespace llvm