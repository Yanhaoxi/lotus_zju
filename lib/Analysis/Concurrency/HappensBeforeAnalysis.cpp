#include "Analysis/Concurrency/HappensBeforeAnalysis.h"
#include <llvm/Support/raw_ostream.h>

using namespace llvm;

namespace lotus {

HappensBeforeAnalysis::HappensBeforeAnalysis(Module &module, mhp::MHPAnalysis &mhp)
    : m_module(module), m_mhp(mhp) {}

void HappensBeforeAnalysis::analyze() {
  // The MHP analysis already builds the Thread Flow Graph (TFG) and computes
  // some ordering. We can leverage that.
  // A full static HB analysis is essentially reachability in the TFG.
  // MHPAnalysis::mustPrecede is exactly this.
}

bool HappensBeforeAnalysis::happensBefore(const Instruction *A, const Instruction *B) const {
  if (!A || !B) return false;
  if (A == B) return true; // Reflexive? Usually HB is irreflexive, but for "ordered before or same"

  // Check cache
  auto key = std::make_pair(A, B);
  if (m_hb_cache.count(key)) {
    return m_hb_cache[key];
  }

  // Use MHP analysis's existing logic which checks TFG reachability and thread ordering
  bool result = m_mhp.mustPrecede(A, B);
  
  m_hb_cache[key] = result;
  return result;
}

} // namespace lotus

