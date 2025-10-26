#ifndef TRANSFORM_LOWERCONSTANTEXPR_H
#define TRANSFORM_LOWERCONSTANTEXPR_H

#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>

using namespace llvm;

// New Pass Manager version
class LowerConstantExprPass : public PassInfoMixin<LowerConstantExprPass> {
public:
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);
};

#endif //TRANSFORM_LOWERCONSTANTEXPR_H
