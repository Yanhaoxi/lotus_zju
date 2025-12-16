/*
 *
 * Author: rainoftime
*/
#include "Dataflow/Mono/DataFlow.h"

namespace mono {

DataFlowAnalysis::DataFlowAnalysis() = default;

std::unique_ptr<DataFlowResult> DataFlowAnalysis::getFullSets(Function *f) {

  if (f == nullptr) {
    return nullptr;
  }

  auto df = std::make_unique<DataFlowResult>();
  for (auto &inst : instructions(*f)) {
    auto &inSetOfInst = df->IN(&inst);
    auto &outSetOfInst = df->OUT(&inst);
    for (auto &inst2 : instructions(*f)) {
      inSetOfInst.insert(&inst2);
      outSetOfInst.insert(&inst2);
    }
  }

  return df;
}

} // namespace mono

