/**
 * \file RemoveDeadBlock.h
 * \brief Pass for removing unreachable basic blocks
 * \author Lotus Team
 *
 * This pass removes basic blocks that are unreachable from the function
 * entry point, cleaning up the control flow graph.
 */
#ifndef TRANSFORM_REMOVEDEADBLOCK_H
#define TRANSFORM_REMOVEDEADBLOCK_H

#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>

using namespace llvm;

/**
 * \class RemoveDeadBlockPass
 * \brief Dead block elimination pass
 *
 * This pass identifies and removes basic blocks that are unreachable
 * from the function entry. This helps:
 * - Clean up the control flow graph
 * - Reduce code size
 * - Improve analysis accuracy by removing unreachable code
 */
class RemoveDeadBlockPass : public PassInfoMixin<RemoveDeadBlockPass> {
public:
  /**
   * \brief Run the dead block removal pass on a module
   * \param M The module to transform
   * \param MAM Module analysis manager
   * \return PreservedAnalyses indicating whether the module was modified
   */
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);
};

#endif // TRANSFORM_REMOVEDEADBLOCK_H
