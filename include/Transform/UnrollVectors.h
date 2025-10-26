#ifndef TRANSFORM_UNROLLVECTORS_H
#define TRANSFORM_UNROLLVECTORS_H

#include <llvm/IR/Function.h>
#include <llvm/IR/PassManager.h>

using namespace llvm;

// New Pass Manager version
class UnrollVectorsPass : public PassInfoMixin<UnrollVectorsPass> {
public:
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM);
};

#endif //TRANSFORM_UNROLLVECTORS_H

