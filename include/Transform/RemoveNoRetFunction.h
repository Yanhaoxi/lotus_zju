#ifndef TRANSFORM_REMOVENORETFUNCTION_H
#define TRANSFORM_REMOVENORETFUNCTION_H

#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>

using namespace llvm;

// New Pass Manager version
class RemoveNoRetFunctionPass : public PassInfoMixin<RemoveNoRetFunctionPass> {
public:
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);
};

#endif //TRANSFORM_REMOVENORETFUNCTION_H
