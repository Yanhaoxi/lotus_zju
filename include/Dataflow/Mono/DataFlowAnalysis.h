
#ifndef ANALYSIS_DATAFLOWANALYSIS_H_
#define ANALYSIS_DATAFLOWANALYSIS_H_

#include "Utils/LLVM/SystemHeaders.h"
#include "Dataflow/Mono/DataFlowResult.h"
#include <memory>

class DataFlowAnalysis {
public:
  /*
   * Methods
   */
  DataFlowAnalysis();

  [[nodiscard]] std::unique_ptr<DataFlowResult> getFullSets(Function *f);
};

#endif // ANALYSIS_DATAFLOWANALYSIS_H_
