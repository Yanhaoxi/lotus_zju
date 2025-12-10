#include "Dataflow/Mono/DataFlowEngine.h"

DataFlowEngine::DataFlowEngine() : AA(nullptr), MSSA(nullptr) {}

DataFlowEngine::DataFlowEngine(AAResults *AA, MemorySSA *MSSA)
    : AA(AA), MSSA(MSSA) {}

std::unique_ptr<DataFlowResult> DataFlowEngine::applyForward(
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
                             DataFlowResult *df)> &computeOUT) {

  /*
   * Define an empty KILL set.
   */
  auto computeKILL = [](Instruction *, DataFlowResult *) {};

  /*
   * Run the data-flow analysis.
   */
  return this->applyForward(f,
                            computeGEN,
                            computeKILL,
                            initializeIN,
                            initializeOUT,
                            computeIN,
                            computeOUT);
}

std::unique_ptr<DataFlowResult> DataFlowEngine::applyForward(
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
                             DataFlowResult *df)> &computeOUT) {

  /*
   * Define the customization.
   */
  auto appendBB = [](std::list<BasicBlock *> &workingList, BasicBlock *bb) {
    workingList.push_back(bb);
  };

  auto getFirstInst = [](BasicBlock *bb) -> Instruction * {
    return &*bb->begin();
  };

  auto getLastInst = [](BasicBlock *bb) -> Instruction * {
    return bb->getTerminator();
  };

  auto getPredecessors = [](BasicBlock *bb) -> std::list<BasicBlock *> {
    std::list<BasicBlock *> Predecessors;
    for (auto predecessor : predecessors(bb)) {
      Predecessors.push_back(predecessor);
    }
    return Predecessors;
  };

  auto getSuccessors = [](BasicBlock *bb) -> std::list<BasicBlock *> {
    std::list<BasicBlock *> Successors;
    for (auto predecessor : successors(bb)) {
      Successors.push_back(predecessor);
    }
    return Successors;
  };

  auto inSetOfInst = [](DataFlowResult *df,
                        Instruction *inst) -> std::set<Value *> & {
    return df->IN(inst);
  };

  auto outSetOfInst = [](DataFlowResult *df,
                         Instruction *inst) -> std::set<Value *> & {
    return df->OUT(inst);
  };

  auto getEndIterator = [](BasicBlock *bb) -> BasicBlock::iterator {
    return bb->end();
  };

  auto incrementIterator = [](BasicBlock::iterator &iter) { iter++; };

  /*
   * Run the pass.
   */

  return this->applyGeneralizedForwardAnalysis(f,
                                               computeGEN,
                                               computeKILL,
                                               initializeIN,
                                               initializeOUT,
                                               getPredecessors,
                                               getSuccessors,
                                               computeIN,
                                               computeOUT,
                                               appendBB,
                                               getFirstInst,
                                               getLastInst,
                                               inSetOfInst,
                                               outSetOfInst,
                                               getEndIterator,
                                               incrementIterator);
}

std::unique_ptr<DataFlowResult> DataFlowEngine::applyBackward(
    Function *f,
    const std::function<void(Instruction *, DataFlowResult *)> &computeGEN,
    const std::function<void(Instruction *, DataFlowResult *)> &computeKILL,
    const std::function<void(Instruction *inst,
                             std::set<Value *> &IN,
                             DataFlowResult *df)> &computeIN,
    const std::function<void(Instruction *inst,
                             Instruction *successor,
                             std::set<Value *> &OUT,
                             DataFlowResult *df)> &computeOUT) {

  /*
   * Define the customization
   */
  auto appendBB = [](std::list<BasicBlock *> &workingList, BasicBlock *bb) {
    workingList.push_front(bb);
  };

  auto getPredecessors = [](BasicBlock *bb) -> std::list<BasicBlock *> {
    std::list<BasicBlock *> Successors;
    for (auto predecessor : successors(bb)) {
      Successors.push_back(predecessor);
    }
    return Successors;
  };

  auto getSuccessors = [](BasicBlock *bb) -> std::list<BasicBlock *> {
    std::list<BasicBlock *> Predecessors;
    for (auto predecessor : predecessors(bb)) {
      Predecessors.push_back(predecessor);
    }
    return Predecessors;
  };

  auto getFirstInst = [](BasicBlock *bb) -> Instruction * {
    return bb->getTerminator();
  };

  auto getLastInst = [](BasicBlock *bb) -> Instruction * {
    return &*bb->begin();
  };

  auto initializeIN = [](Instruction *, std::set<Value *> &) {};

  auto initializeOUT = [](Instruction *, std::set<Value *> &) {};

  auto inSetOfInst = [](DataFlowResult *df,
                        Instruction *inst) -> std::set<Value *> & {
    return df->OUT(inst);
  };

  auto outSetOfInst = [](DataFlowResult *df,
                         Instruction *inst) -> std::set<Value *> & {
    return df->IN(inst);
  };

  auto getEndIterator = [](BasicBlock *bb) -> BasicBlock::iterator {
    return bb->begin();
  };

  auto incrementIterator = [](BasicBlock::iterator &iter) { iter--; };

  return this->applyGeneralizedForwardAnalysis(f,
                                               computeGEN,
                                               computeKILL,
                                               initializeIN,
                                               initializeOUT,
                                               getPredecessors,
                                               getSuccessors,
                                               computeOUT,
                                               computeIN,
                                               appendBB,
                                               getFirstInst,
                                               getLastInst,
                                               inSetOfInst,
                                               outSetOfInst,
                                               getEndIterator,
                                               incrementIterator);
}

void DataFlowEngine::computeGENAndKILL(
    Function *f,
    const std::function<void(Instruction *, DataFlowResult *)> &computeGEN,
    const std::function<void(Instruction *, DataFlowResult *)> &computeKILL,
    DataFlowResult *df) {

  /*
   * Compute the GENs and KILLs
   */
  for (auto &bb : *f) {
    for (auto &i : bb) {
      computeGEN(&i, df);
      computeKILL(&i, df);
    }
  }

  return;
}

std::unique_ptr<DataFlowResult> DataFlowEngine::applyGeneralizedForwardAnalysis(
    Function *f,
    const std::function<void(Instruction *, DataFlowResult *)> &computeGEN,
    const std::function<void(Instruction *, DataFlowResult *)> &computeKILL,
    const std::function<void(Instruction *inst, std::set<Value *> &IN)>
        &initializeIN,
    const std::function<void(Instruction *inst, std::set<Value *> &OUT)>
        &initializeOUT,
    const std::function<std::list<BasicBlock *>(BasicBlock *bb)>
        &getPredecessors,
    const std::function<std::list<BasicBlock *>(BasicBlock *bb)> &getSuccessors,
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
    const std::function<void(BasicBlock::iterator &)> &incrementIterator) {

  if (f == nullptr) {
    return nullptr;
  }

  /*
   * Initialize IN and OUT sets.
   */
  auto df = std::make_unique<DataFlowResult>();
  for (auto &bb : *f) {
    for (auto &i : bb) {
      auto &INSet = df->IN(&i);
      auto &OUTSet = df->OUT(&i);
      initializeIN(&i, INSet);
      initializeOUT(&i, OUTSet);
    }
  }

  /*
   * Compute the GENs and KILLs
   */
  computeGENAndKILL(f, computeGEN, computeKILL, df.get());

  /*
   * Compute the IN and OUT
   *
   * Create the working list by adding all basic blocks to it.
   */
  std::list<BasicBlock *> workingList;
  std::unordered_set<BasicBlock *> inWorkList;
  for (auto &bb : *f) {
    appendBB(workingList, &bb);
    inWorkList.insert(&bb);
  }

  /*
   * Compute the INs and OUTs iteratively until the working list is empty.
   */
  std::unordered_set<BasicBlock *> computedOnce;

  while (!workingList.empty()) {

    /*
     * Fetch a basic block that needs to be processed.
     */
    auto bb = workingList.front();

    /*
     * Remove the basic block from the workingList.
     */
    workingList.pop_front();
    inWorkList.erase(bb);

    /*
     * Fetch the first instruction of the basic block.
     */
    auto inst = getFirstInstruction(bb);

    /*
     * Fetch IN[inst], OUT[inst], GEN[inst], and KILL[inst]
     */
    auto &inSetOfInst = getInSetOfInst(df.get(), inst);
    auto &outSetOfInst = getOutSetOfInst(df.get(), inst);

    /*
     * Compute the IN of the first instruction of the current basic block.
     */
    for (auto predecessorBB : getPredecessors(bb)) {

      /*
       * Fetch the current predecessor of "inst".
       */
      auto predecessorInst = getLastInstruction(predecessorBB);

      /*
       * Compute IN[inst]
       */
      computeIN(inst, predecessorInst, inSetOfInst, df.get());
    }

    /*
     * Compute OUT[inst]
     */
    const auto previousOut = outSetOfInst;
    computeOUT(inst, outSetOfInst, df.get());

    /* Check if the OUT of the first instruction of the current basic block
     * changed.
     */
    const bool firstVisit = computedOnce.insert(bb).second;
    const bool outChanged = previousOut != outSetOfInst;
    if (firstVisit || outChanged) {

      /*
       * Propagate the new OUT[inst] to the rest of the instructions of the
       * current basic block.
       */
      BasicBlock::iterator iter(inst);
      auto predI = cast<Instruction>(inst);
      while (iter != getEndIterator(bb)) {

        /*
         * Move the iterator.
         */
        incrementIterator(iter);

        /*
         * Fetch the current instruction.
         */
        auto i = &*iter;

        /*
         * Compute IN[i]
         */
        auto &inSetOfI = getInSetOfInst(df.get(), i);
        computeIN(i, predI, inSetOfI, df.get());

        /*
         * Compute OUT[i]
         */
        auto &outSetOfI = getOutSetOfInst(df.get(), i);
        computeOUT(i, outSetOfI, df.get());

        /*
         * Update the predecessor.
         */
        predI = i;
      }

      /*
       * Add successors of the current basic block to the working list.
       */
      for (auto succBB : getSuccessors(bb)) {
        if (inWorkList.insert(succBB).second) {
          workingList.push_back(succBB);
        }
      }
    }
  }

  return df;
}

