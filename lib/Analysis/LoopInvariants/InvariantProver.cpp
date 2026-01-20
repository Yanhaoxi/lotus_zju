#include "Analysis/LoopInvariants/InvariantProver.h"

#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"

#include "z3++.h"

using namespace llvm;
using namespace lotus;

std::string InvariantProver::getValueName(const Value *V) {
  return V->hasName() ? V->getName().str() : "var";
}

InvariantProver::InvariantProver(const Loop &Loop, ScalarEvolution &SE,
                                 DominatorTree &DT)
    : L(Loop), SE(SE), DT(DT), Ctx(&Z3Expr::getContext()) {}

InvariantProver::~InvariantProver() = default;

z3::expr InvariantProver::getInitialValue(const PHINode *Phi) {
  BasicBlock *Preheader = L.getLoopPreheader();
  if (!Preheader)
    return Ctx->int_val(0);

  Value *PreheaderVal = Phi->getIncomingValueForBlock(Preheader);
  if (!PreheaderVal)
    return Ctx->int_val(0);

  const SCEV *S = SE.getSCEV(PreheaderVal);
  if (const SCEVConstant *SC = dyn_cast<SCEVConstant>(S)) {
    const APInt &Val = SC->getAPInt();
    if (Val.getBitWidth() <= 64) {
      return Ctx->int_val(Val.getSExtValue());
    }
  }

  return Ctx->int_val(0);
}

z3::expr InvariantProver::getStepValue(const PHINode *Phi) {
  const SCEV *PhiSCEV = SE.getSCEV(const_cast<PHINode *>(Phi));
  if (isa<SCEVCouldNotCompute>(PhiSCEV))
    return Ctx->int_val(0);

  const SCEVAddRecExpr *AR = dyn_cast<SCEVAddRecExpr>(PhiSCEV);
  if (!AR || AR->getLoop() != &L)
    return Ctx->int_val(0);

  const SCEV *Step = AR->getStepRecurrence(SE);

  if (const SCEVConstant *SC = dyn_cast<SCEVConstant>(Step)) {
    const APInt &Val = SC->getAPInt();
    if (Val.getBitWidth() <= 64) {
      return Ctx->int_val(Val.getSExtValue());
    }
  }

  return Ctx->int_val(0);
}

void InvariantProver::buildBaseCaseConstraints(z3::solver &Solver) {
  BasicBlock *Preheader = L.getLoopPreheader();
  if (!Preheader)
    return;

  BasicBlock *Header = L.getHeader();

  for (auto &Inst : *Header) {
    PHINode *Phi = dyn_cast<PHINode>(&Inst);
    if (!Phi)
      break;

    const SCEV *PhiSCEV = SE.getSCEV(Phi);
    if (isa<SCEVCouldNotCompute>(PhiSCEV))
      continue;

    const SCEVAddRecExpr *AR = dyn_cast<SCEVAddRecExpr>(PhiSCEV);
    if (!AR || AR->getLoop() != &L)
      continue;

    std::string PhiName = getValueName(Phi);
    z3::expr PhiVar = Ctx->int_const(PhiName.c_str());
    z3::expr InitVal = getInitialValue(Phi);

    Solver.add(PhiVar == InitVal);
  }
}

void InvariantProver::buildStepCaseConstraints(z3::solver &Solver) {
  BasicBlock *Header = L.getHeader();

  for (auto &Inst : *Header) {
    PHINode *Phi = dyn_cast<PHINode>(&Inst);
    if (!Phi)
      break;

    const SCEV *PhiSCEV = SE.getSCEV(Phi);
    if (isa<SCEVCouldNotCompute>(PhiSCEV))
      continue;

    const SCEVAddRecExpr *AR = dyn_cast<SCEVAddRecExpr>(PhiSCEV);
    if (!AR || AR->getLoop() != &L)
      continue;

    const SCEV *Step = AR->getStepRecurrence(SE);

    int64_t StepVal = 0;
    bool HasStepVal = false;

    if (const SCEVConstant *SC = dyn_cast<SCEVConstant>(Step)) {
      const APInt &Val = SC->getAPInt();
      if (Val.getBitWidth() <= 64) {
        StepVal = Val.getSExtValue();
        HasStepVal = true;
      }
    }

    if (HasStepVal && StepVal != 0) {
      std::string PhiName = getValueName(Phi);
      z3::expr PhiVar = Ctx->int_const(PhiName.c_str());
      z3::expr StepExpr = Ctx->int_val(StepVal);

      z3::expr PhiNext = PhiVar + StepExpr;
      z3::expr PhiNextVar = Ctx->int_const((PhiName + "_next").c_str());

      Solver.add(PhiNextVar == PhiNext);
    }
  }
}

InvariantProver::ProofResult
InvariantProver::proveInvariant(const InvariantCandidate &Candidate) {
  if (Candidate.Formula.id() == 0)
    return ProofResult(false, "Invalid formula");

  z3::solver Solver(*Ctx);

  z3::expr InvariantToProve = Candidate.Formula.getExpr();

  if (Candidate.IsImplication && Candidate.Premise.id() != 0) {
    z3::expr Premise = Candidate.Premise.getExpr();
    z3::expr Conclusion = Candidate.Formula.getExpr();
    InvariantToProve = (!Premise) || Conclusion;
    llvm::errs() << "DEBUG: Proving implication: (!premise) || conclusion\n";
  }

  ProofResult BaseResult = proveBase(InvariantToProve, Solver);
  if (!BaseResult.IsProven)
    return BaseResult;

  Solver.reset();

  ProofResult StepResult = proveStep(InvariantToProve, Solver);
  return StepResult;
}

InvariantProver::ProofResult
InvariantProver::proveBase(const z3::expr &Invariant, z3::solver &Solver) {
  Solver.push();

  buildBaseCaseConstraints(Solver);

  llvm::errs() << "DEBUG: Base constraints added\n";

  Solver.add(Invariant);

  z3::expr NotInv = !Invariant;
  Solver.add(NotInv);

  z3::check_result Result = Solver.check();
  llvm::errs() << "DEBUG: Base case solver result: ";
  if (Result == z3::unsat) {
    llvm::errs() << "UNSAT\n";
  } else if (Result == z3::sat) {
    llvm::errs() << "SAT (invariant can be violated)\n";
    z3::model Model = Solver.get_model();
    llvm::errs() << "DEBUG: Model: " << Model << "\n";
  } else {
    llvm::errs() << "UNKNOWN\n";
  }

  Solver.pop();

  if (Result == z3::unsat) {
    return ProofResult(true);
  } else if (Result == z3::sat) {
    return ProofResult(false,
                       "Base case fails: invariant does not hold at entry");
  } else {
    return ProofResult(false, "Base case unknown");
  }
}

InvariantProver::ProofResult
InvariantProver::proveStep(const z3::expr &Invariant, z3::solver &Solver) {
  Solver.push();

  Solver.add(Invariant);

  buildStepCaseConstraints(Solver);

  z3::expr NotInv = !Invariant;
  Solver.add(NotInv);

  z3::check_result Result = Solver.check();

  Solver.pop();

  if (Result == z3::unsat) {
    return ProofResult(true);
  } else if (Result == z3::sat) {
    return ProofResult(false, "Step case fails: invariant not preserved");
  } else {
    return ProofResult(false, "Step case unknown");
  }
}
