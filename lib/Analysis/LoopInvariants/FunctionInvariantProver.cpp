#include "Analysis/LoopInvariants/FunctionInvariantProver.h"

#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"

#include "z3++.h"

using namespace llvm;
using namespace lotus;

std::string FunctionInvariantProver::getValueName(const Value *V) {
  return V->hasName() ? V->getName().str() : "var";
}

FunctionInvariantProver::FunctionInvariantProver(const Function &F,
                                                 ScalarEvolution &SE)
    : Func(F), SE(SE), Ctx(&Z3Expr::getContext()) {}

FunctionInvariantProver::~FunctionInvariantProver() = default;

FunctionInvariantProver::ProofResult FunctionInvariantProver::proveInvariant(
    const FunctionInvariantCandidate &Candidate) {
  if (Candidate.Formula.id() == 0)
    return ProofResult(false, "Invalid formula");

  z3::solver Solver(*Ctx);

  ProofResult ExitResult = proveAtExit(Candidate.Formula.getExpr(), Solver);
  return ExitResult;
}

FunctionInvariantProver::ProofResult
FunctionInvariantProver::proveAtExit(const z3::expr &Invariant,
                                     z3::solver &Solver) {
  Solver.push();

  for (const BasicBlock &BB : Func) {
    for (const Instruction &I : BB) {
      if (const PHINode *Phi = dyn_cast<PHINode>(&I)) {
        std::string PhiName = getValueName(Phi);
        z3::expr PhiVar = Ctx->int_const(PhiName.c_str());

        const SCEV *PhiSCEV = SE.getSCEV(const_cast<PHINode *>(Phi));
        if (isa<SCEVCouldNotCompute>(PhiSCEV))
          continue;

        if (const SCEVConstant *SC = dyn_cast<SCEVConstant>(PhiSCEV)) {
          const APInt &Val = SC->getAPInt();
          if (Val.getBitWidth() <= 64) {
            z3::expr InitVal = Ctx->int_val(Val.getSExtValue());
            Solver.add(PhiVar == InitVal);
          }
        }
      }
    }
  }

  Solver.add(Invariant);

  z3::expr NotInv = !Invariant;
  Solver.add(NotInv);

  z3::check_result Result = Solver.check();

  llvm::errs() << "DEBUG: Function exit case solver result: ";
  if (Result == z3::unsat) {
    llvm::errs() << "UNSAT\n";
  } else if (Result == z3::sat) {
    llvm::errs() << "SAT (invariant can be violated at exit)\n";
    z3::model Model = Solver.get_model();
    llvm::errs() << "DEBUG: Model: " << Model << "\n";
  } else {
    llvm::errs() << "UNKNOWN\n";
  }

  Solver.pop();

  if (Result == z3::unsat) {
    return ProofResult(true);
  } else if (Result == z3::sat) {
    return ProofResult(
        false, "Exit case fails: invariant does not hold at function exit");
  } else {
    return ProofResult(false, "Exit case unknown");
  }
}
