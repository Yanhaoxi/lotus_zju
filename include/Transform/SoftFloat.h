#ifndef TRANSFORM_SOFTFLOAT_H
#define TRANSFORM_SOFTFLOAT_H

#include <llvm/IR/Function.h>
#include <llvm/IR/PassManager.h>

using namespace llvm;

// New Pass Manager version
class SoftFloatPass : public PassInfoMixin<SoftFloatPass> {
public:
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM);
};

#endif //TRANSFORM_SOFTFLOAT_H

