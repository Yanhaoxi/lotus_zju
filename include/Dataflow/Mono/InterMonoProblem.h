#ifndef ANALYSIS_INTERMONOPROBLEM_H_
#define ANALYSIS_INTERMONOPROBLEM_H_

#include "Dataflow/Mono/IntraMonoProblem.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Value.h"
#include "llvm/ADT/ArrayRef.h"

namespace mono {

template <typename AnalysisDomainTy>
class InterMonoProblem : public IntraMonoProblem<AnalysisDomainTy> {
public:
  using n_t = typename AnalysisDomainTy::n_t;
  using f_t = typename AnalysisDomainTy::f_t;
  using mono_container_t = typename AnalysisDomainTy::mono_container_t;

  explicit InterMonoProblem(std::vector<llvm::Function *> EntryPoints = {})
      : IntraMonoProblem<AnalysisDomainTy>(std::move(EntryPoints)) {}

  virtual mono_container_t callFlow(n_t CallSite, f_t Callee,
                                    const mono_container_t &In) = 0;
  virtual mono_container_t returnFlow(n_t CallSite, f_t Callee, n_t ExitStmt,
                                      n_t RetSite,
                                      const mono_container_t &In) = 0;
  virtual mono_container_t callToRetFlow(
      n_t CallSite, n_t RetSite, llvm::ArrayRef<f_t> Callees,
      const mono_container_t &In) = 0;
};

} // namespace mono

#endif // ANALYSIS_INTERMONOPROBLEM_H_
