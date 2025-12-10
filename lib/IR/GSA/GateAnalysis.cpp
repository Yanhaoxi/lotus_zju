//===- GateAnalysis.cpp - Gated SSA construction --------------------------===//
//
// Builds the Gated SSA (GSA) representation by materializing gating functions
// (gamma nodes) for existing PHI nodes. The transformation optionally replaces
// PHI nodes with the computed gammas and can emit a thinned version that
// reduces the use of undef values.
//
// The implementation is adapted from Havlak's construction of Thinned Gated
// Single-Assignment form, LCPC'93.
//
//===----------------------------------------------------------------------===//

#include "IR/GSA/GSA.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include <set>
#include <vector>

using namespace llvm;

namespace gsa {

static cl::opt<bool> ThinnedGsa("gsa-thinned",
                                cl::desc("Emit thin gamma nodes (TGSA)"),
                                cl::init(true), cl::Hidden);

static cl::opt<bool> GsaReplacePhis(
    "gsa-replace-phis",
    cl::desc("Replace PHI nodes with gamma nodes in the IR"), cl::init(true),
    cl::Hidden);

namespace {

class GateAnalysisImpl : public GateAnalysis {
  Function &m_function;
  DominatorTree &m_DT;
  PostDominatorTree &m_PDT;
  ControlDependenceAnalysis &m_CDA;

  DenseMap<PHINode *, Value *> m_gammas;
  IRBuilder<> m_IRB;
  bool m_changed{false};

public:
  GateAnalysisImpl(Function &f, DominatorTree &dt, PostDominatorTree &pdt,
                   ControlDependenceAnalysis &cda)
      : m_function(f), m_DT(dt), m_PDT(pdt), m_CDA(cda), m_IRB(f.getContext()) {
    calculate();
  }

  Value *getGamma(PHINode *PN) const override {
    auto it = m_gammas.find(PN);
    assert(it != m_gammas.end());
    return it->second;
  }

  bool isThinned() const override { return ThinnedGsa; }

  bool madeChanges() const { return m_changed; }

private:
  void calculate();
  void processPhi(PHINode *PN, Instruction *insertionPt);
  DenseMap<BasicBlock *, Value *>
  processIncomingValues(PHINode *PN, Instruction *insertionPt);
};

Value *GetCondition(Instruction *TI) {
  if (auto *BI = dyn_cast<BranchInst>(TI)) {
    assert(BI->isConditional() && "Unconditional branches cannot be gates!");
    return BI->getCondition();
  }

  if (auto *SI = dyn_cast<SwitchInst>(TI)) {
    assert(SI->getNumCases() > 1 && "Unconditional switches cannot be gates!");
    return SI->getCondition();
  }

  llvm_unreachable("Unhandled terminator instruction");
}

void GateAnalysisImpl::calculate() {
  std::vector<PHINode *> phis;
  DenseMap<BasicBlock *, Instruction *> insertionPts;

  // Gammas need to be placed just after the last PHI nodes. This is because
  // LLVM utilities expect PHIs to appear at the very beginning of basic blocks.
  for (auto &BB : m_function) {
    Instruction *insertionPoint = BB.getFirstNonPHI();
    assert(insertionPoint && "Basic block without terminator?");
    insertionPts.insert({&BB, insertionPoint});

    for (auto &PN : BB.phis())
      phis.push_back(&PN);
  }

  for (PHINode *PN : phis) {
    auto *BB = PN->getParent();
    processPhi(PN, insertionPts[BB]);
  }
}

// Construct gating functions for incoming critical edges in the GSA mode.
// Construct a mapping between incoming blocks to values.
DenseMap<BasicBlock *, Value *>
GateAnalysisImpl::processIncomingValues(PHINode *PN, Instruction *insertionPt) {
  assert(PN);

  BasicBlock *const currentBB = PN->getParent();
  m_IRB.SetInsertPoint(insertionPt);
  UndefValue *const Undef = UndefValue::get(PN->getType());

  DenseMap<BasicBlock *, Value *> incomingBlockToValue;
  for (unsigned i = 0, e = PN->getNumIncomingValues(); i != e; ++i) {
    BasicBlock *incomingBlock = PN->getIncomingBlock(i);
    Value *incomingValue = PN->getIncomingValue(i);
    incomingBlockToValue[incomingBlock] = incomingValue;

    auto *TI = incomingBlock->getTerminator();
    assert(isa<BranchInst>(TI) && "Other terminators not supported yet");
    auto *BI = dyn_cast<BranchInst>(TI);
    if (BI->isUnconditional())
      continue;

    Value *cond = GetCondition(TI);
    BasicBlock *trueDest = BI->getSuccessor(0);
    BasicBlock *falseDest = BI->getSuccessor(1);
    assert(trueDest == currentBB || falseDest == currentBB);

    if (!ThinnedGsa) {
      Value *SI = m_IRB.CreateSelect(
          cond, trueDest == currentBB ? incomingValue : Undef,
          falseDest == currentBB ? incomingValue : Undef,
          {"seahorn.gsa.gamma.crit.", incomingBlock->getName()});
      m_changed = true;
      incomingBlockToValue[incomingBlock] = SI;
    }
  }

  return incomingBlockToValue;
}

void GateAnalysisImpl::processPhi(PHINode *PN, Instruction *insertionPt) {
  assert(PN);

  BasicBlock *const currentBB = PN->getParent();
  DenseMap<BasicBlock *, Value *> incomingBlockToValue =
      processIncomingValues(PN, insertionPt);

  // Make sure CD blocks are sorted in reverse-topological order. We need this
  // because we want to process them in order opposite to execution order.
  auto GreaterThanTopo = [this](BasicBlock *first, BasicBlock *second) {
    return m_CDA.getBBTopoIdx(first) > m_CDA.getBBTopoIdx(second);
  };

  std::set<BasicBlock *, decltype(GreaterThanTopo)> cdInfo(GreaterThanTopo);
  // Collect all blocks the incoming blocks are control dependent on.
  for (unsigned i = 0, e = PN->getNumIncomingValues(); i != e; ++i) {
    auto *BB = PN->getIncomingBlock(i);
    for (BasicBlock *CDBlock : m_CDA.getCDBlocks(BB))
      cdInfo.insert(CDBlock);
  }

  // Mapping from blocks in cdInfo to values potentially guarded by gammas.
  DenseMap<BasicBlock *, Value *> flowingValues(incomingBlockToValue.begin(),
                                                incomingBlockToValue.end());

  Type *const phiTy = PN->getType();
  UndefValue *const Undef = UndefValue::get(phiTy);
  m_IRB.SetInsertPoint(insertionPt);

  // For all blocks in cdInfo inspect their successors to construct gamma nodes
  // where needed.
  for (BasicBlock *BB : cdInfo) {
    auto *TI = BB->getTerminator();
    assert(isa<BranchInst>(TI) && "Only BranchInst is supported right now");

    // Collect all successors and associated values that flows when they are
    // taken (or Undef if no such flow exists).
    SmallDenseMap<BasicBlock *, Value *, 2> SuccToVal;
    for (auto *S : successors(BB)) {
      SuccToVal[S] = Undef;

      // Direct branch to the PHI's parent block.
      if (S == currentBB)
        SuccToVal[S] = incomingBlockToValue[BB];

      if (SuccToVal[S] != Undef)
        continue;

      // Or the successor unconditionally flows to an already processed block.
      // Note that there can be at most one such block.
      BasicBlock *postDomBlock = S;
      while (postDomBlock) {
        auto it = flowingValues.find(postDomBlock);
        if (it != flowingValues.end()) {
          SuccToVal[S] = it->second;
          break;
        }
        auto *treeNode = m_PDT.getNode(postDomBlock)->getIDom();
        if (treeNode == nullptr)
          break;
        postDomBlock = treeNode->getBlock();
      }
    }

    auto *BI = cast<BranchInst>(TI);
    assert(1 <= SuccToVal.size() && SuccToVal.size() <= 2);
    if (SuccToVal.size() == 1) {
      auto &SuccValPair = *SuccToVal.begin();
      assert(SuccValPair.second != Undef);
      flowingValues[BB] = SuccValPair.second;
    } else if (SuccToVal.size() == 2) {
      BasicBlock *TrueDest = BI->getSuccessor(0);
      BasicBlock *FalseDest = BI->getSuccessor(1);
      Value *TrueVal = SuccToVal[TrueDest];
      Value *FalseVal = SuccToVal[FalseDest];

      // Construct gamma node only when necessary.
      if (TrueVal == Undef && FalseVal == Undef) {
        flowingValues[BB] = Undef;
      } else if (TrueVal == FalseVal) {
        flowingValues[BB] = TrueVal;
      } else if (ThinnedGsa && (TrueVal == Undef || FalseVal == Undef)) {
        flowingValues[BB] = FalseVal == Undef ? TrueVal : FalseVal;
      } else {
        // Gammas are expressed as SelectInsts and placed in the analyzed IR.
        Value *Ite =
            m_IRB.CreateSelect(BI->getCondition(), TrueVal, FalseVal,
                               {"seahorn.gsa.gamma.", BB->getName()});
        m_changed = true;
        flowingValues[BB] = Ite;
      }
    }
  }

  auto *domNode = m_DT.getNode(currentBB);
  assert(domNode && domNode->getIDom() && "PHI in entry block is unexpected");
  BasicBlock *IDomBlock = domNode->getIDom()->getBlock();
  assert(IDomBlock);
  assert(flowingValues.count(IDomBlock));

  Value *gamma = flowingValues[IDomBlock];
  gamma->setName(gamma->getName() + ".y." + PN->getName());
  m_gammas[PN] = gamma;

  if (GsaReplacePhis) {
    PN->replaceAllUsesWith(gamma);
    PN->eraseFromParent();
    m_changed = true;
  }
}

} // anonymous namespace

char GateAnalysisPass::ID = 0;

GateAnalysisPass::GateAnalysisPass() : ModulePass(ID) {}

void GateAnalysisPass::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<ControlDependenceAnalysisPass>();
  AU.addRequired<DominatorTreeWrapperPass>();
  AU.addRequired<PostDominatorTreeWrapperPass>();
  AU.setPreservesAll();
}

bool GateAnalysisPass::runOnModule(Module &M) {
  auto &CDP = getAnalysis<ControlDependenceAnalysisPass>();
  bool changed = false;

  for (auto &F : M) {
    if (F.isDeclaration())
      continue;
    changed |= runOnFunction(F, CDP.getControlDependenceAnalysis(F));
  }

  if (GsaReplacePhis) {
    for (auto &F : M) {
      if (F.isDeclaration())
        continue;
      for (auto &BB : F)
        for (auto &I : BB) {
          (void)&I;
          assert(!isa<PHINode>(I) && "All PHI nodes should be replaced in GSA");
        }
    }
  }

  return changed;
}

bool GateAnalysisPass::runOnFunction(llvm::Function &F,
                                     ControlDependenceAnalysis &CDA) {
  auto &DT = getAnalysis<DominatorTreeWrapperPass>(F).getDomTree();
  auto &PDT = getAnalysis<PostDominatorTreeWrapperPass>(F).getPostDomTree();

  auto Impl = std::make_unique<GateAnalysisImpl>(F, DT, PDT, CDA);
  bool changed = Impl->madeChanges();

  m_analyses[&F] = std::move(Impl);
  return changed;
}

llvm::StringRef GateAnalysisPass::getPassName() const {
  return "GateAnalysisPass";
}

void GateAnalysisPass::print(raw_ostream &os, const llvm::Module *) const {
  os << "GateAnalysisPass::print\n";
}

bool GateAnalysisPass::hasAnalysisFor(const llvm::Function &F) const {
  return m_analyses.count(&F) > 0;
}

GateAnalysis &GateAnalysisPass::getGateAnalysis(const llvm::Function &F) {
  assert(hasAnalysisFor(F));
  return *m_analyses[&F];
}

llvm::ModulePass *createGateAnalysisPass() { return new GateAnalysisPass(); }

} // namespace gsa

static llvm::RegisterPass<gsa::GateAnalysisPass>
    GsaGA("gsa-gated-ssa", "Compute Gated SSA form", true, true);

