#ifndef TRANSFORM_SIMPLIFYLATCH_H
#define TRANSFORM_SIMPLIFYLATCH_H

#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>

using namespace llvm;

// New Pass Manager version
class SimplifyLatchPass : public PassInfoMixin<SimplifyLatchPass> {
public:
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);
};

#endif //TRANSFORM_SIMPLIFYLATCH_H
