#include "Analysis/LoopInvariants/FunctionInvariantCandidateGenerator.h"

#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
//#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace lotus;

FunctionInvariantCandidateGenerator::FunctionInvariantCandidateGenerator(
    const Function &F, ScalarEvolution &SE)
    : Func(F), SE(SE) {}

void FunctionInvariantCandidateGenerator::generateCandidates(
    SmallVectorImpl<FunctionInvariantCandidate> &Candidates) {

  collectReturnValues();

  generateReturnBoundInvariants(Candidates);
  generateReturnNonNegativeInvariants(Candidates);
  generateReturnComparisonInvariants(Candidates);
  generateReturnPlusComponentInvariants(Candidates);
  generateReturnMinusNonNegativeInvariants(Candidates);
}

void FunctionInvariantCandidateGenerator::collectReturnValues() {
  for (const BasicBlock &BB : Func) {
    for (const Instruction &I : BB) {
      if (const ReturnInst *RI = dyn_cast<ReturnInst>(&I)) {
        ReturnInsts.push_back(RI);

        if (Value *RetVal = RI->getReturnValue()) {
          ReturnValues.push_back(RetVal);
        }
      }
    }
  }
}

void FunctionInvariantCandidateGenerator::generateReturnBoundInvariants(
    SmallVectorImpl<FunctionInvariantCandidate> &Candidates) {
  for (const Value *RetVal : ReturnValues) {
    const SCEV *RetSCEV = SE.getSCEV(const_cast<Value *>(RetVal));
    if (isa<SCEVCouldNotCompute>(RetSCEV))
      continue;

    if (const SCEVConstant *SC = dyn_cast<SCEVConstant>(RetSCEV)) {
      const APInt &Val = SC->getAPInt();
      if (Val.getBitWidth() <= 64 && Val.isNonNegative()) {
        Z3Expr RetExpr = valueToZ3Expr(RetVal);
        Z3Expr ZeroExpr = Z3Expr(0);

        FunctionInvariantCandidate Candidate(
            FunctionInvariantCandidate::ReturnBound);
        Candidate.Formula = RetExpr >= ZeroExpr;
        Candidate.Description = "Return value is non-negative";
        Candidate.InvolvedValues.push_back(RetVal);
        Candidates.push_back(Candidate);
      }
    }
  }
}

void FunctionInvariantCandidateGenerator::generateReturnNonNegativeInvariants(
    SmallVectorImpl<FunctionInvariantCandidate> &Candidates) {
  for (const Value *RetVal : ReturnValues) {
    if (const BinaryOperator *BO = dyn_cast<BinaryOperator>(RetVal)) {
      unsigned OpCode = BO->getOpcode();

      if (OpCode == Instruction::Add || OpCode == Instruction::FAdd) {
        Z3Expr RetExpr = valueToZ3Expr(RetVal);
        Z3Expr ZeroExpr = Z3Expr(0);

        FunctionInvariantCandidate Candidate(
            FunctionInvariantCandidate::ReturnNonNegative);
        Candidate.Formula = RetExpr >= ZeroExpr;
        Candidate.Description = "Addition result is non-negative";
        Candidate.InvolvedValues.push_back(RetVal);
        Candidates.push_back(Candidate);
      }

      if (OpCode == Instruction::Sub || OpCode == Instruction::FSub) {
        Z3Expr RetExpr = valueToZ3Expr(RetVal);
        Z3Expr ZeroExpr = Z3Expr(0);

        FunctionInvariantCandidate Candidate(
            FunctionInvariantCandidate::ReturnNonNegative);
        Candidate.Formula = RetExpr >= ZeroExpr;
        Candidate.Description = "Subtraction result is non-negative";
        Candidate.InvolvedValues.push_back(RetVal);
        Candidates.push_back(Candidate);
      }
    }
  }
}

void FunctionInvariantCandidateGenerator::generateReturnComparisonInvariants(
    SmallVectorImpl<FunctionInvariantCandidate> &Candidates) {
  for (const Value *RetVal : ReturnValues) {
    if (const BinaryOperator *BO = dyn_cast<BinaryOperator>(RetVal)) {
      unsigned OpCode = BO->getOpcode();

      if (OpCode == Instruction::Sub || OpCode == Instruction::FSub) {
        Value *Op0 = BO->getOperand(0);
        Value *Op1 = BO->getOperand(1);

        Z3Expr RetExpr = valueToZ3Expr(RetVal);
        Z3Expr Op0Expr = valueToZ3Expr(Op0);
        Z3Expr Op1Expr = valueToZ3Expr(Op1);

        FunctionInvariantCandidate Candidate(
            FunctionInvariantCandidate::ReturnComparison);

        Candidate.Formula = RetExpr >= Op1Expr;
        Candidate.Description = "Return value >= second operand of subtraction";
        Candidate.InvolvedValues.push_back(RetVal);
        Candidate.InvolvedValues.push_back(Op1);
        Candidates.push_back(Candidate);
      }
    }
  }
}

void FunctionInvariantCandidateGenerator::generateReturnPlusComponentInvariants(
    SmallVectorImpl<FunctionInvariantCandidate> &Candidates) {
  for (const Value *RetVal : ReturnValues) {
    if (const BinaryOperator *BO = dyn_cast<BinaryOperator>(RetVal)) {
      unsigned OpCode = BO->getOpcode();

      if (OpCode == Instruction::Add || OpCode == Instruction::FAdd) {
        Value *Op0 = BO->getOperand(0);
        Value *Op1 = BO->getOperand(1);

        Z3Expr RetExpr = valueToZ3Expr(RetVal);
        Z3Expr Op0Expr = valueToZ3Expr(Op0);
        Z3Expr Op1Expr = valueToZ3Expr(Op1);

        {
          FunctionInvariantCandidate Candidate(
              FunctionInvariantCandidate::ReturnPlusComponents);
          Candidate.Formula = RetExpr >= Op0Expr;
          Candidate.Description = "Return value >= first operand of addition";
          Candidate.InvolvedValues.push_back(RetVal);
          Candidate.InvolvedValues.push_back(Op0);
          Candidates.push_back(Candidate);
        }

        {
          FunctionInvariantCandidate Candidate(
              FunctionInvariantCandidate::ReturnPlusComponents);
          Candidate.Formula = RetExpr >= Op1Expr;
          Candidate.Description = "Return value >= second operand of addition";
          Candidate.InvolvedValues.push_back(RetVal);
          Candidate.InvolvedValues.push_back(Op1);
          Candidates.push_back(Candidate);
        }
      }
    }
  }
}

void FunctionInvariantCandidateGenerator::
    generateReturnMinusNonNegativeInvariants(
        SmallVectorImpl<FunctionInvariantCandidate> &Candidates) {
  for (const Value *RetVal : ReturnValues) {
    if (const BinaryOperator *BO = dyn_cast<BinaryOperator>(RetVal)) {
      unsigned OpCode = BO->getOpcode();

      if (OpCode == Instruction::Sub || OpCode == Instruction::FSub) {
        Z3Expr RetExpr = valueToZ3Expr(RetVal);
        Z3Expr ZeroExpr = Z3Expr(0);

        FunctionInvariantCandidate Candidate(
            FunctionInvariantCandidate::ReturnMinusNonNegative);
        Candidate.Formula = RetExpr >= ZeroExpr;
        Candidate.Description = "Return value from subtraction is non-negative";
        Candidate.InvolvedValues.push_back(RetVal);
        Candidates.push_back(Candidate);
      }
    }
  }
}

Z3Expr FunctionInvariantCandidateGenerator::valueToZ3Expr(const Value *V) {
  std::string VarName = V->hasName() ? V->getName().str() : "ret";
  z3::context &Ctx = Z3Expr::getContext();
  return Z3Expr(Ctx.int_const(VarName.c_str()));
}

Z3Expr FunctionInvariantCandidateGenerator::scevToZ3Expr(const SCEV *S) {
  if (const SCEVConstant *SC = dyn_cast<SCEVConstant>(S)) {
    const APInt &Val = SC->getAPInt();
    if (Val.getBitWidth() <= 64) {
      return Z3Expr(static_cast<int>(Val.getSExtValue()));
    }
  }

  return Z3Expr::getFalseCond();
}
