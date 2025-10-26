#ifndef TRANSFORM_NAMEBLOCK_H
#define TRANSFORM_NAMEBLOCK_H

#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>

using namespace llvm;

// New Pass Manager version
class NameBlockPass : public PassInfoMixin<NameBlockPass> {
public:
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);
};

#endif //TRANSFORM_NAMEBLOCK_H
