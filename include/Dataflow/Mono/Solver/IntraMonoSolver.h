#ifndef ANALYSIS_INTRAMONOSOLVER_H_
#define ANALYSIS_INTRAMONOSOLVER_H_

#include "Dataflow/Mono/IntraMonoProblem.h"

#include "llvm/IR/CFG.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"

#include <deque>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mono {

template <typename AnalysisDomainTy> class IntraMonoSolver {
public:
  using ProblemTy = IntraMonoProblem<AnalysisDomainTy>;
  using n_t = typename AnalysisDomainTy::n_t;
  using mono_container_t = typename AnalysisDomainTy::mono_container_t;

  explicit IntraMonoSolver(ProblemTy &Problem) : Problem(Problem) {}

  void solve() {
    initialize();
    while (!Worklist.empty()) {
      auto Edge = Worklist.front();
      Worklist.pop_front();
      auto Src = Edge.first;
      auto Dst = Edge.second;

      auto Out = Problem.normalFlow(Src, AnalysisIn[Src]);
      if (isBranchTarget(Dst)) {
        for (auto Pred : getPredsOf(Dst)) {
          if (Pred == Src) {
            continue;
          }
          auto OtherOut = Problem.normalFlow(Pred, AnalysisIn[Pred]);
          Out = Problem.merge(Out, OtherOut);
        }
      }

      if (!Problem.equal_to(Out, AnalysisIn[Dst])) {
        AnalysisIn[Dst] = Out;
        for (auto Succ : getSuccsOf(Dst)) {
          Worklist.push_back({Dst, Succ});
        }
      }
    }

    for (auto &Entry : AnalysisIn) {
      AnalysisOut[Entry.first] = Problem.normalFlow(Entry.first, Entry.second);
    }
  }

  [[nodiscard]] const mono_container_t &getInResultsAt(n_t Stmt) const {
    auto It = AnalysisIn.find(Stmt);
    if (It != AnalysisIn.end()) {
      return It->second;
    }
    return DefaultValue;
  }

  [[nodiscard]] const mono_container_t &getOutResultsAt(n_t Stmt) const {
    auto It = AnalysisOut.find(Stmt);
    if (It != AnalysisOut.end()) {
      return It->second;
    }
    return DefaultValue;
  }

  [[nodiscard]] const std::unordered_map<n_t, mono_container_t> &
  getInResults() const {
    return AnalysisIn;
  }

  [[nodiscard]] const std::unordered_map<n_t, mono_container_t> &
  getOutResults() const {
    return AnalysisOut;
  }

private:
  void initialize() {
    for (auto *Function : Problem.getEntryPoints()) {
      if (Function == nullptr || Function->isDeclaration()) {
        continue;
      }

      auto Edges = getAllControlFlowEdges(Function);
      Worklist.insert(Worklist.begin(), Edges.begin(), Edges.end());

      for (auto &BB : *Function) {
        for (auto &Inst : BB) {
          AnalysisIn.insert({&Inst, Problem.allTop()});
        }
      }
    }

    for (const auto &Entry : Problem.initialSeeds()) {
      AnalysisIn[Entry.first] = Entry.second;
    }
  }

  std::vector<std::pair<n_t, n_t>>
  getAllControlFlowEdges(llvm::Function *Function) const {
    std::vector<std::pair<n_t, n_t>> Edges;
    for (auto &BB : *Function) {
      for (auto &Inst : BB) {
        for (auto *Succ : getSuccsOf(&Inst)) {
          Edges.push_back({&Inst, Succ});
        }
      }
    }
    return Edges;
  }

  std::vector<n_t> getSuccsOf(n_t Inst) const {
    return Problem.direction() == FlowDirection::Forward
               ? getForwardSuccs(Inst)
               : getBackwardSuccs(Inst);
  }

  std::vector<n_t> getPredsOf(n_t Inst) const {
    return Problem.direction() == FlowDirection::Forward
               ? getBackwardSuccs(Inst)
               : getForwardSuccs(Inst);
  }

  static std::vector<n_t> getForwardSuccs(n_t Inst) {
    std::vector<n_t> Succs;
    if (Inst->isTerminator()) {
      for (auto *SuccBB : llvm::successors(Inst->getParent())) {
        Succs.push_back(&*SuccBB->begin());
      }
      return Succs;
    }
    if (auto *Next = Inst->getNextNode()) {
      Succs.push_back(Next);
    }
    return Succs;
  }

  static std::vector<n_t> getBackwardSuccs(n_t Inst) {
    std::vector<n_t> Preds;
    auto *BB = Inst->getParent();
    if (Inst != &*BB->begin()) {
      Preds.push_back(Inst->getPrevNode());
      return Preds;
    }
    for (auto *PredBB : llvm::predecessors(BB)) {
      Preds.push_back(PredBB->getTerminator());
    }
    return Preds;
  }

  bool isBranchTarget(n_t Inst) const { return getPredsOf(Inst).size() > 1; }

  ProblemTy &Problem;
  std::deque<std::pair<n_t, n_t>> Worklist;
  std::unordered_map<n_t, mono_container_t> AnalysisIn;
  std::unordered_map<n_t, mono_container_t> AnalysisOut;
  mono_container_t DefaultValue{};
};

} // namespace mono

#endif // ANALYSIS_INTRAMONOSOLVER_H_
