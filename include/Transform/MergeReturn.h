#ifndef TRANSFORM_MERGERETURN_H
#define TRANSFORM_MERGERETURN_H

#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>

using namespace llvm;

// New Pass Manager version
class MergeReturnPass : public PassInfoMixin<MergeReturnPass> {
public:
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);
};

#endif //TRANSFORM_MERGERETURN_H
