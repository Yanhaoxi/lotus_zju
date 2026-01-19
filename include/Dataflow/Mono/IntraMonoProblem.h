#ifndef ANALYSIS_INTRAMONOPROBLEM_H_
#define ANALYSIS_INTRAMONOPROBLEM_H_

#include "Dataflow/Mono/FlowDirection.h"

#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace mono {

template <typename AnalysisDomainTy> class IntraMonoProblem {
public:
  using n_t = typename AnalysisDomainTy::n_t;
  using mono_container_t = typename AnalysisDomainTy::mono_container_t;

  explicit IntraMonoProblem(std::vector<llvm::Function *> EntryPoints = {})
      : EntryPoints(std::move(EntryPoints)) {}

  virtual ~IntraMonoProblem() = default;

  virtual mono_container_t normalFlow(n_t Inst,
                                      const mono_container_t &In) = 0;
  virtual mono_container_t merge(const mono_container_t &Lhs,
                                 const mono_container_t &Rhs) = 0;
  virtual bool equal_to(const mono_container_t &Lhs,
                        const mono_container_t &Rhs) = 0;

  virtual mono_container_t allTop() { return mono_container_t{}; }
  virtual std::unordered_map<n_t, mono_container_t> initialSeeds() = 0;
  virtual FlowDirection direction() const { return FlowDirection::Forward; }

  virtual void printContainer(llvm::raw_ostream &,
                              const mono_container_t &) const {}

  [[nodiscard]] const std::vector<llvm::Function *> &getEntryPoints() const {
    return EntryPoints;
  }

protected:
  std::vector<llvm::Function *> EntryPoints;
};

} // namespace mono

#endif // ANALYSIS_INTRAMONOPROBLEM_H_
