
#ifndef ANALYSIS_DATAFLOWENGINE_H_
#define ANALYSIS_DATAFLOWENGINE_H_

#include "Dataflow/Mono/DataFlowResult.h"
#include <functional>
#include <list>
#include <memory>
#include <set>

// Forward declarations for optional memory analysis support
namespace llvm {
  class AAResults;
  class MemorySSA;
}

class DataFlowEngine {
public:
  /*
   * Constructors
   */
  DataFlowEngine();
  
  // Constructor with optional alias analysis and memory SSA support
  DataFlowEngine(AAResults *AA, MemorySSA *MSSA = nullptr);
  
  /*
   * Accessors for optional analysis results
   */
  AAResults *getAliasAnalysis() const { return AA; }
  MemorySSA *getMemorySSA() const { return MSSA; }
  bool hasAliasAnalysis() const { return AA != nullptr; }
  bool hasMemorySSA() const { return MSSA != nullptr; }

  [[nodiscard]] std::unique_ptr<DataFlowResult> applyForward(
      Function *f,
      const std::function<void(Instruction *, DataFlowResult *)> &computeGEN,
      const std::function<void(Instruction *, DataFlowResult *)> &computeKILL,
      const std::function<void(Instruction *inst, std::set<Value *> &IN)>
          &initializeIN,
      const std::function<void(Instruction *inst, std::set<Value *> &OUT)>
          &initializeOUT,
      const std::function<void(Instruction *inst,
                               Instruction *predecessor,
                               std::set<Value *> &IN,
                               DataFlowResult *df)> &computeIN,
      const std::function<void(Instruction *inst,
                               std::set<Value *> &OUT,
                               DataFlowResult *df)> &computeOUT);

  [[nodiscard]] std::unique_ptr<DataFlowResult> applyForward(
      Function *f,
      const std::function<void(Instruction *, DataFlowResult *)> &computeGEN,
      const std::function<void(Instruction *inst, std::set<Value *> &IN)>
          &initializeIN,
      const std::function<void(Instruction *inst, std::set<Value *> &OUT)>
          &initializeOUT,
      const std::function<void(Instruction *inst,
                               Instruction *predecessor,
                               std::set<Value *> &IN,
                               DataFlowResult *df)> &computeIN,
      const std::function<void(Instruction *inst,
                               std::set<Value *> &OUT,
                               DataFlowResult *df)> &computeOUT);

  [[nodiscard]] std::unique_ptr<DataFlowResult> applyBackward(
      Function *f,
      const std::function<void(Instruction *, DataFlowResult *)> &computeGEN,
      const std::function<void(Instruction *, DataFlowResult *)> &computeKILL,
      const std::function<void(Instruction *inst,
                               std::set<Value *> &IN,
                               DataFlowResult *df)> &computeIN,
      const std::function<void(Instruction *inst,
                               Instruction *successor,
                               std::set<Value *> &OUT,
                               DataFlowResult *df)> &computeOUT);

protected:
  void computeGENAndKILL(
      Function *f,
      const std::function<void(Instruction *, DataFlowResult *)> &computeGEN,
      const std::function<void(Instruction *, DataFlowResult *)> &computeKILL,
      DataFlowResult *df);

private:
  [[nodiscard]] std::unique_ptr<DataFlowResult> applyGeneralizedForwardAnalysis(
      Function *f,
      const std::function<void(Instruction *, DataFlowResult *)> &computeGEN,
      const std::function<void(Instruction *, DataFlowResult *)> &computeKILL,
      const std::function<void(Instruction *inst, std::set<Value *> &IN)>
          &initializeIN,
      const std::function<void(Instruction *inst, std::set<Value *> &OUT)>
          &initializeOUT,
      const std::function<std::list<BasicBlock *>(BasicBlock *bb)>
          &getPredecessors,
      const std::function<std::list<BasicBlock *>(BasicBlock *bb)>
          &getSuccessors,
      const std::function<void(Instruction *inst,
                               Instruction *predecessor,
                               std::set<Value *> &IN,
                               DataFlowResult *df)> &computeIN,
      const std::function<void(Instruction *inst,
                               std::set<Value *> &OUT,
                               DataFlowResult *df)> &computeOUT,
      const std::function<void(std::list<BasicBlock *> &workingList,
                               BasicBlock *bb)> &appendBB,
      const std::function<Instruction *(BasicBlock *bb)> &getFirstInstruction,
      const std::function<Instruction *(BasicBlock *bb)> &getLastInstruction,
      const std::function<std::set<Value *> &(DataFlowResult *df,
                                              Instruction *instruction)>
          &getInSetOfInst,
      const std::function<std::set<Value *> &(DataFlowResult *df,
                                              Instruction *instruction)>
          &getOutSetOfInst,
      const std::function<BasicBlock::iterator(BasicBlock *)> &getEndIterator,
      const std::function<void(BasicBlock::iterator &)> &incrementIterator);

  /*
   * Optional analysis results for memory-aware analyses
   */
  AAResults *AA;
  MemorySSA *MSSA;
};

#endif // ANALYSIS_DATAFLOWENGINE_H_
