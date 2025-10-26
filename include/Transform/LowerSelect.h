#ifndef TRANSFORM_LOWERSELECT_H
#define TRANSFORM_LOWERSELECT_H

#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>

using namespace llvm;

// New Pass Manager version
class LowerSelectPass : public PassInfoMixin<LowerSelectPass> {
public:
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);
};

#endif //TRANSFORM_LOWERSELECT_H
