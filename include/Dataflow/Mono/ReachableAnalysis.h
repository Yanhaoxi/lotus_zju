
#ifndef ANALYSIS_REACHABLEANALYSIS_H_
#define ANALYSIS_REACHABLEANALYSIS_H_

#include "Dataflow/Mono/DataFlowResult.h"
#include <functional>
#include <memory>

// Compute forward reachability using backward dataflow analysis.
// This analysis determines which instructions can be executed from each program point.
[[nodiscard]] std::unique_ptr<DataFlowResult> runReachableAnalysis(Function *f);

[[nodiscard]] std::unique_ptr<DataFlowResult> runReachableAnalysis(
    Function *f,
    const std::function<bool(Instruction *i)> &filter);

#endif // ANALYSIS_REACHABLEANALYSIS_H_

