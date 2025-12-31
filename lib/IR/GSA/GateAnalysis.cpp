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
// Author: rainoftime
//===----------------------------------------------------------------------===//

#include "llvm/ADT/DenseMap.h"

#include "IR/GSA/GSA.h"
// #include "llvm/ADT/DenseSet.h"
#include "llvm/Analysis/LoopInfo.h"
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

static Value *getBottom(Type *Ty) {
  assert(Ty && "Type must be provided for bottom value");
  return PoisonValue::get(Ty);
}

static bool dominatesForUse(Value *V, Instruction *UseI, DominatorTree &DT) {
  if (!UseI)
    return false;
  if (isa<Argument>(V) || isa<Constant>(V))
    return true;
  auto *Inst = dyn_cast<Instruction>(V);
  if (!Inst)
    return false;
  if (Inst->getParent() == UseI->getParent())
    return Inst->comesBefore(UseI);
  return DT.dominates(Inst, UseI);
}

static cl::opt<bool> ThinnedGsa("gsa-thinned",
                                cl::desc("Emit thin gamma nodes (TGSA)"),
                                cl::init(true), cl::Hidden);

static cl::opt<bool>
    GsaReplacePhis("gsa-replace-phis",
                   cl::desc("Replace PHI nodes with gamma nodes in the IR"),
                   cl::init(true), cl::Hidden);

namespace {

class GateAnalysisImpl : public GateAnalysis {
  Function &m_function;
  DominatorTree &m_DT;
  PostDominatorTree &m_PDT;
  LoopInfo &m_LI;
  ControlDependenceAnalysis &m_CDA;

  DenseMap<PHINode *, Value *> m_gammas;
  IRBuilder<> m_IRB;
  bool m_changed{false};

public:
  GateAnalysisImpl(Function &f, DominatorTree &dt, PostDominatorTree &pdt,
                   LoopInfo &li, ControlDependenceAnalysis &cda)
      : m_function(f), m_DT(dt), m_PDT(pdt), m_LI(li), m_CDA(cda),
        m_IRB(f.getContext()) {
    calculate();
  }

  Value *getGamma(PHINode *PN) const override {
    auto it = m_gammas.find(PN);
    assert(it != m_gammas.end());
    return it->second;
  }

  bool isMu(PHINode *PN) const override {
    return m_LI.isLoopHeader(PN->getParent());
  }

  bool isEta(PHINode *PN) const override {
    BasicBlock *BB = PN->getParent();
    Loop *L = m_LI.getLoopFor(BB);
    for (unsigned i = 0, e = PN->getNumIncomingValues(); i != e; ++i) {
      BasicBlock *IncBB = PN->getIncomingBlock(i);
      Loop *IncL = m_LI.getLoopFor(IncBB);
      if (IncL && IncL != L && IncL->contains(IncBB) &&
          (!L || !L->contains(IncBB))) {
        // Incoming from a loop that does not contain the PHI's block
        // This is an exit PHI (Eta)
        return true;
      }
    }
    return false;
  }

  bool isThinned() const override { return ThinnedGsa; }

  bool madeChanges() const { return m_changed; }

private:
  void calculate();
  void processPhi(PHINode *PN, Instruction *insertionPt);
  DenseMap<BasicBlock *, Value *>
  processIncomingValues(PHINode *PN, Instruction *insertionPt);
};

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
  Value *const Bottom = getBottom(PN->getType());

  DenseMap<BasicBlock *, Value *> incomingBlockToValue;
  for (unsigned i = 0, e = PN->getNumIncomingValues(); i != e; ++i) {
    BasicBlock *incomingBlock = PN->getIncomingBlock(i);
    Value *incomingValue = PN->getIncomingValue(i);

    if (!dominatesForUse(incomingValue, insertionPt, m_DT))
      incomingValue = Bottom;

    incomingBlockToValue[incomingBlock] = incomingValue;

    if (ThinnedGsa)
      continue;

    auto *TI = incomingBlock->getTerminator();

    if (auto *BI = dyn_cast<BranchInst>(TI)) {
      if (BI->isUnconditional())
        continue;

      if (!m_DT.dominates(incomingBlock, currentBB) ||
          !dominatesForUse(BI->getCondition(), insertionPt, m_DT))
        continue;

      Value *EdgePred = BI->getSuccessor(0) == currentBB
                            ? BI->getCondition()
                            : m_IRB.CreateNot(BI->getCondition(),
                                              Twine("seahorn.gsa.edge.") +
                                                  incomingBlock->getName());

      Value *Guarded = m_IRB.CreateSelect(
          EdgePred, incomingValue, Bottom,
          {"seahorn.gsa.gamma.crit.", incomingBlock->getName()});
      incomingBlockToValue[incomingBlock] = Guarded;
      m_changed = true;
      continue;
    }

    if (auto *SI = dyn_cast<SwitchInst>(TI)) {
      if (!m_DT.dominates(incomingBlock, currentBB) ||
          !dominatesForUse(SI->getCondition(), insertionPt, m_DT))
        continue;

      Value *EdgePred = m_IRB.getFalse();
      Value *AnyCase = m_IRB.getFalse();

      for (auto Case : SI->cases()) {
        Value *Cmp = m_IRB.CreateICmpEQ(SI->getCondition(), Case.getCaseValue(),
                                        Twine("seahorn.gsa.case.") +
                                            incomingBlock->getName());
        AnyCase = m_IRB.CreateOr(AnyCase, Cmp);
        if (Case.getCaseSuccessor() == currentBB)
          EdgePred = m_IRB.CreateOr(EdgePred, Cmp,
                                    Twine("seahorn.gsa.edge.case.") +
                                        incomingBlock->getName());
      }

      if (SI->getDefaultDest() == currentBB)
        EdgePred = m_IRB.CreateOr(EdgePred, m_IRB.CreateNot(AnyCase),
                                  Twine("seahorn.gsa.edge.default.") +
                                      incomingBlock->getName());

      if (auto *ConstPred = dyn_cast<ConstantInt>(EdgePred))
        if (ConstPred->isZero())
          continue;

      Value *Guarded = m_IRB.CreateSelect(
          EdgePred, incomingValue, Bottom,
          {"seahorn.gsa.gamma.crit.", incomingBlock->getName()});
      incomingBlockToValue[incomingBlock] = Guarded;
      m_changed = true;
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
  Value *const Bottom = getBottom(phiTy);
  m_IRB.SetInsertPoint(insertionPt);

  // For all blocks in cdInfo inspect their successors to construct gamma nodes
  // where needed.
  for (BasicBlock *BB : cdInfo) {
    auto *TI = BB->getTerminator();
    if (!TI) {
      flowingValues[BB] = Bottom;
      continue;
    }

    // Collect all successors and associated values that flows when they are
    // taken (or Bottom if no such flow exists).
    SmallDenseMap<BasicBlock *, Value *, 2> SuccToVal;
    for (auto *S : successors(BB)) {
      SuccToVal[S] = Bottom;

      // Direct branch to the PHI's parent block.
      if (S == currentBB) {
        auto IncomingIt = incomingBlockToValue.find(BB);
        SuccToVal[S] = IncomingIt != incomingBlockToValue.end()
                           ? IncomingIt->second
                           : Bottom;
      }

      if (SuccToVal[S] != Bottom)
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

    if (auto *BI = dyn_cast<BranchInst>(TI)) {
      if (SuccToVal.empty()) {
        flowingValues[BB] = Bottom;
        continue;
      }

      if (SuccToVal.size() == 1) {
        auto &SuccValPair = *SuccToVal.begin();
        flowingValues[BB] = SuccValPair.second;
        continue;
      }

      BasicBlock *TrueDest = BI->getSuccessor(0);
      BasicBlock *FalseDest = BI->getSuccessor(1);
      Value *TrueVal = SuccToVal[TrueDest];
      Value *FalseVal = SuccToVal[FalseDest];

      // Construct gamma node only when necessary and only if the condition
      // dominates the insertion point.
      if (TrueVal == Bottom && FalseVal == Bottom) {
        flowingValues[BB] = Bottom;
      } else if (TrueVal == FalseVal) {
        flowingValues[BB] = TrueVal;
      } else if (ThinnedGsa && (TrueVal == Bottom || FalseVal == Bottom)) {
        flowingValues[BB] = FalseVal == Bottom ? TrueVal : FalseVal;
      } else if (dominatesForUse(BI->getCondition(), insertionPt, m_DT)) {
        Value *Ite = m_IRB.CreateSelect(BI->getCondition(), TrueVal, FalseVal,
                                        {"seahorn.gsa.gamma.", BB->getName()});
        m_changed = true;
        flowingValues[BB] = Ite;
      } else {
        flowingValues[BB] = Bottom;
      }
      continue;
    }

    if (auto *SI = dyn_cast<SwitchInst>(TI)) {
      if (SuccToVal.empty()) {
        flowingValues[BB] = Bottom;
        continue;
      }

      // If the switch condition is not available at the insertion point, fall
      // back conservatively.
      if (!dominatesForUse(SI->getCondition(), insertionPt, m_DT)) {
        flowingValues[BB] = Bottom;
        continue;
      }

      Value *CaseMatched = m_IRB.getFalse();
      Value *Accum = nullptr;
      for (auto Case : SI->cases()) {
        Value *Cmp = m_IRB.CreateICmpEQ(SI->getCondition(), Case.getCaseValue(),
                                        Twine("seahorn.gsa.gamma.case.") +
                                            BB->getName());
        CaseMatched = m_IRB.CreateOr(CaseMatched, Cmp);
        BasicBlock *Succ = Case.getCaseSuccessor();
        Value *SuccVal = SuccToVal.lookup(Succ);
        if (!SuccVal || SuccVal == Bottom)
          continue;
        Value *Base = Accum ? Accum : Bottom;
        Accum = m_IRB.CreateSelect(Cmp, SuccVal, Base,
                                   Twine("seahorn.gsa.gamma.") + BB->getName() +
                                       ".case");
        m_changed = true;
      }

      BasicBlock *DefaultDest = SI->getDefaultDest();
      Value *DefaultVal = SuccToVal.lookup(DefaultDest);
      if (DefaultVal && DefaultVal != Bottom) {
        Value *DefaultTaken = m_IRB.CreateNot(
            CaseMatched, Twine("seahorn.gsa.gamma.default.") + BB->getName());
        Value *Base = Accum ? Accum : Bottom;
        Accum = m_IRB.CreateSelect(DefaultTaken, DefaultVal, Base,
                                   Twine("seahorn.gsa.gamma.") + BB->getName() +
                                       ".default");
        m_changed = true;
      }

      flowingValues[BB] = Accum ? Accum : Bottom;
      continue;
    }

    // Unsupported terminator shapes fall back to Bottom to keep the
    // transformation conservative and avoid invalid IR.
    flowingValues[BB] = Bottom;
  }

  auto *domNode = m_DT.getNode(currentBB);
  assert(domNode && domNode->getIDom() && "PHI in entry block is unexpected");
  BasicBlock *IDomBlock = domNode->getIDom()->getBlock();
  assert(IDomBlock);
  Value *gamma =
      flowingValues.count(IDomBlock) ? flowingValues[IDomBlock] : Bottom;
  if (auto *I = dyn_cast<Instruction>(gamma)) {
    if (isMu(PN))
      I->setName(I->getName() + ".m." + PN->getName());
    else if (isEta(PN))
      I->setName(I->getName() + ".e." + PN->getName());
    else
      I->setName(I->getName() + ".y." + PN->getName());
  }
  m_gammas[PN] = gamma;

  if (GsaReplacePhis && gamma != Bottom) {
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
  AU.addRequired<LoopInfoWrapperPass>();
  AU.setPreservesAll();
}

bool GateAnalysisPass::runOnModule(Module &M) {
  auto &CDP = getAnalysis<ControlDependenceAnalysisPass>();
  bool changed = false;

  for (auto &F : M) {
    if (F.isDeclaration())
      continue;
    auto &LI = getAnalysis<LoopInfoWrapperPass>(F).getLoopInfo();
    changed |= runOnFunction(F, CDP.getControlDependenceAnalysis(F), LI);
  }

  if (GsaReplacePhis) {
    for (auto &F : M) {
      if (F.isDeclaration())
        continue;
      for (auto &BB : F)
        for (auto &I : BB)
          (void)&I;
    }
  }

  return changed;
}

bool GateAnalysisPass::runOnFunction(llvm::Function &F,
                                     ControlDependenceAnalysis &CDA,
                                     llvm::LoopInfo &LI) {
  auto &DT = getAnalysis<DominatorTreeWrapperPass>(F).getDomTree();
  auto &PDT = getAnalysis<PostDominatorTreeWrapperPass>(F).getPostDomTree();

  auto Impl = std::make_unique<GateAnalysisImpl>(F, DT, PDT, LI, CDA);
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
