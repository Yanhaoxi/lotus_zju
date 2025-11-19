#ifndef HAPPENS_BEFORE_ANALYSIS_H
#define HAPPENS_BEFORE_ANALYSIS_H

#include "Analysis/Concurrency/MHPAnalysis.h"
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Module.h>
#include <unordered_map>
#include <vector>

namespace lotus {

class HappensBeforeAnalysis {
public:
  explicit HappensBeforeAnalysis(llvm::Module &module, mhp::MHPAnalysis &mhp);

  void analyze();

  /**
   * @brief Check if instruction A happens-before instruction B
   * @param A The first instruction
   * @param B The second instruction
   * @return true if A happens-before B
   */
  bool happensBefore(const llvm::Instruction *A, const llvm::Instruction *B) const;

private:
  llvm::Module &m_module;
  mhp::MHPAnalysis &m_mhp;
  
  // Vector clock or similar timestamp mechanism could be used here
  // For static analysis, we often use graph reachability on the TFG
  
  // Cache for HB queries
  struct InstPairHash {
    size_t operator()(const std::pair<const llvm::Instruction *, const llvm::Instruction *> &p) const {
      return std::hash<const llvm::Instruction *>()(p.first) ^ std::hash<const llvm::Instruction *>()(p.second);
    }
  };
  mutable std::unordered_map<std::pair<const llvm::Instruction *, const llvm::Instruction *>, bool, InstPairHash> m_hb_cache;
};

} // namespace lotus

#endif // HAPPENS_BEFORE_ANALYSIS_H

