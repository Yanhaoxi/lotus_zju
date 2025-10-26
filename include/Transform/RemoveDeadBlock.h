#ifndef TRANSFORM_REMOVEDEADBLOCK_H
#define TRANSFORM_REMOVEDEADBLOCK_H

#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>

using namespace llvm;

// New Pass Manager version
class RemoveDeadBlockPass : public PassInfoMixin<RemoveDeadBlockPass> {
public:
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);
};

#endif //TRANSFORM_REMOVEDEADBLOCK_H
