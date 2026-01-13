// Implementation of FunctionTranslator.
//
// Translates a single LLVM Function into a TPA Control Flow Graph (CFG).
//
// Process:
// 1. Basic Block Translation: Iterates over instructions, translating each relevant one
//    into a `CFGNode` via `InstructionTranslator`.
// 2. CFG Construction: Connects the translated nodes to form the graph structure.
//    Handles empty blocks (blocks with no relevant pointer instructions) by stitching
//    predecessors directly to successors.
// 3. Def-Use Analysis: Explicitly builds def-use chains for pointer values.
//    (e.g., connecting an Alloc node to a Store node that uses it).
// 4. Cleanup: Detaches store-preserving nodes (Alloc, Copy, Offset) from the control-flow
//    graph, leaving them only connected via def-use chains. This transforms the CFG
//    into a "Semi-Sparse" representation where only memory-accessing nodes (Load, Store, Call)
//    are sequenced in control flow.

#include "Alias/TPA/PointerAnalysis/FrontEnd/CFG/FunctionTranslator.h"

#include "Alias/TPA/PointerAnalysis/FrontEnd/CFG/InstructionTranslator.h"
#include "Alias/TPA/PointerAnalysis/FrontEnd/CFG/PriorityAssigner.h"
#include "Alias/TPA/PointerAnalysis/Program/CFG/CFG.h"
#include "Alias/TPA/Util/Log.h"

#include <llvm/IR/CFG.h>
#include <llvm/IR/Function.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;

namespace tpa {

void FunctionTranslator::translateBasicBlock(const Function &llvmFunc) {
  for (auto const &currBlock : llvmFunc) {
    tpa::CFGNode *startNode = nullptr;
    tpa::CFGNode *endNode = nullptr;

    for (auto const &inst : currBlock) {
      auto *currNode = translator.visit(const_cast<Instruction &>(inst));
      if (currNode == nullptr)
        continue;
      else {
        instToNode[&inst] = currNode;
        nodeToInst[currNode] = &inst;
      }

      // Update the first node
      if (startNode == nullptr)
        startNode = currNode;
      // Chain the node with the last one
      if (endNode != nullptr)
        endNode->insertEdge(currNode);
      endNode = currNode;
    }

    assert((startNode == nullptr) == (endNode == nullptr));
    if (startNode != nullptr)
      bbToNode.insert(
          std::make_pair(&currBlock, std::make_pair(startNode, endNode)));
    else
      nonEmptySuccMap[&currBlock] = std::vector<tpa::CFGNode *>();
  }
}

// TODO: This function contains some legacy codes. Refactoring needed
// Handling empty blocks (blocks that became empty after filtering irrelevant instructions).
// We need to find the nearest non-empty successors to connect the graph correctly.
void FunctionTranslator::processEmptyBlock() {
  auto processedEmptyBlock = SmallPtrSet<const BasicBlock *, 32>();
  for (auto &mapping : nonEmptySuccMap) {
    const auto *currBlock = mapping.first;
    auto succs = SmallPtrSet<tpa::CFGNode *, 16>();

    auto workList = std::vector<const BasicBlock *>();
    workList.insert(workList.end(), succ_begin(currBlock), succ_end(currBlock));
    auto visitedEmptyBlock = SmallPtrSet<const BasicBlock *, 16>();
    visitedEmptyBlock.insert(currBlock);

    while (!workList.empty()) {
      const auto *nextBlock = workList.back();
      workList.pop_back();

      if (bbToNode.count(nextBlock)) {
        succs.insert(bbToNode[nextBlock].first);
      } else if (processedEmptyBlock.count(nextBlock)) {
        auto &nextVec = nonEmptySuccMap[nextBlock];
        succs.insert(nextVec.begin(), nextVec.end());
      } else {
        for (auto itr = succ_begin(nextBlock), ite = succ_end(nextBlock);
             itr != ite; ++itr) {
          const auto *nextNextBlock = *itr;
          if (visitedEmptyBlock.count(nextNextBlock))
            continue;
          if (!bbToNode.count(nextNextBlock))
            visitedEmptyBlock.insert(nextNextBlock);
          workList.push_back(nextNextBlock);
        }
      }
    }

    processedEmptyBlock.insert(currBlock);
    mapping.second.insert(mapping.second.end(), succs.begin(), succs.end());
  }
}

void FunctionTranslator::connectCFGNodes(const BasicBlock &entryBlock) {
  for (auto &mapping : bbToNode) {
    auto *bb = mapping.first;
    auto *lastNode = mapping.second.second;

    for (auto itr = succ_begin(bb), ite = succ_end(bb); itr != ite; ++itr) {
      auto *nextBB = *itr;
      auto bbItr = bbToNode.find(nextBB);
      if (bbItr != bbToNode.end())
        lastNode->insertEdge(bbItr->second.first);
      else {
        assert(nonEmptySuccMap.count(nextBB));
        auto &vec = nonEmptySuccMap[nextBB];
        for (auto *succNode : vec)
          lastNode->insertEdge(succNode);
      }
    }
  }

  // Connect the entry node with the main graph
  if (bbToNode.count(&entryBlock))
    cfg.getEntryNode()->insertEdge(bbToNode[&entryBlock].first);
  else {
    assert(nonEmptySuccMap.count(&entryBlock));
    auto &vec = nonEmptySuccMap[&entryBlock];
    for (auto *node : vec)
      cfg.getEntryNode()->insertEdge(node);
  }
}

void FunctionTranslator::drawDefUseEdgeFromValue(const Value *defVal,
                                                 tpa::CFGNode *useNode) {
  assert(defVal != nullptr && useNode != nullptr);

  if (!defVal->getType()->isPointerTy())
    return;

  if (isa<GlobalValue>(defVal) || isa<Argument>(defVal) ||
      isa<UndefValue>(defVal) || isa<ConstantPointerNull>(defVal)) {
    // Nodes that use global values are def roots
    cfg.getEntryNode()->insertDefUseEdge(useNode);
  } else if (const auto *defInst = dyn_cast<Instruction>(defVal)) {
    // For instructions, see if we have corresponding node attached to it
    if (auto *defNode = instToNode[defInst])
      defNode->insertDefUseEdge(useNode);
    else {
      std::string instStr;
      raw_string_ostream instOS(instStr);
      instOS << *defInst;
      instOS.flush();
      LOG_WARN("Failed to find node for instruction: {}", instStr);
    }
  }
}

void FunctionTranslator::constructDefUseChains() {
  for (auto *useNode : cfg) {
    switch (useNode->getNodeTag()) {
    case CFGNodeTag::Entry:
      break;
    case CFGNodeTag::Alloc:
      cfg.getEntryNode()->insertDefUseEdge(useNode);
      break;
    case CFGNodeTag::Copy: {
      const auto *copyNode = static_cast<const CopyCFGNode *>(useNode);
      for (auto *src : *copyNode) {
        const auto *defVal = src->stripPointerCasts();
        drawDefUseEdgeFromValue(defVal, useNode);
      }
      break;
    }
    case CFGNodeTag::Offset: {
      const auto *offsetNode = static_cast<const OffsetCFGNode *>(useNode);
      const auto *defVal = offsetNode->getSrc()->stripPointerCasts();
      drawDefUseEdgeFromValue(defVal, useNode);
      break;
    }
    case CFGNodeTag::Load: {
      const auto *loadNode = static_cast<const LoadCFGNode *>(useNode);
      const auto *defVal = loadNode->getSrc()->stripPointerCasts();
      drawDefUseEdgeFromValue(defVal, useNode);
      break;
    }
    case CFGNodeTag::Store: {
      const auto *storeNode = static_cast<const StoreCFGNode *>(useNode);
      const auto *srcVal = storeNode->getSrc()->stripPointerCasts();
      drawDefUseEdgeFromValue(srcVal, useNode);
      const auto *dstVal = storeNode->getDest()->stripPointerCasts();
      drawDefUseEdgeFromValue(dstVal, useNode);
      break;
    }
    case CFGNodeTag::Call: {
      const auto *callNode = static_cast<const CallCFGNode *>(useNode);
      const auto *funPtr = callNode->getFunctionPointer()->stripPointerCasts();
      drawDefUseEdgeFromValue(funPtr, useNode);
      for (auto *arg : *callNode) {
        const auto *defVal = arg->stripPointerCasts();
        drawDefUseEdgeFromValue(defVal, useNode);
      }
      break;
    }
    case CFGNodeTag::Ret: {
      const auto *retNode = static_cast<const ReturnCFGNode *>(useNode);
      const auto *retVal = retNode->getReturnValue();
      if (retVal != nullptr) {
        const auto *defVal = retVal->stripPointerCasts();
        drawDefUseEdgeFromValue(defVal, useNode);
      }
      break;
    }
    }
  }
}

void FunctionTranslator::computeNodePriority() {
  PriorityAssigner pa(cfg);
  pa.traverse();
}

// "Semi-Sparse" optimization:
// Nodes that only manipulate top-level pointers (Alloc, Copy, Offset) do not
// affect the store directly. They are "sparse" in the sense that they don't participate
// in the memory flow directly. We detach them from the CFG, so the flow analysis
// skips them, relying purely on def-use chains for their values.
void FunctionTranslator::detachStorePreservingNodes() {
  for (auto *node : cfg) {
    if (node->isAllocNode() || node->isCopyNode() || node->isOffsetNode())
      node->detachFromCFG();
  }
}

void FunctionTranslator::translateFunction(const Function &llvmFunc) {
  // Scan the basic blocks and create the nodes first. We will worry about how
  // to connect them later
  translateBasicBlock(llvmFunc);

  // Now the biggest problem are those "empty blocks" (i.e. blocks that do not
  // contain any tpa::CFGNode). Those blocks may form cycles. So we need to
  // know, in advance, what are the non empty successors of the empty blocks.
  processEmptyBlock();

  // Connect all the cfg nodes we've built
  connectCFGNodes(llvmFunc.getEntryBlock());

  // Draw def-use edges
  constructDefUseChains();

  // Compute the priority of each node
  computeNodePriority();

  // Detach all store-preserving nodes
  detachStorePreservingNodes();
}

} // namespace tpa