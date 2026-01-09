add_library(CanaryOptimization STATIC
  AInliner.cpp
  ModuleOptimizer.cpp
  SWPrefetching.cpp
  LICM.cpp
  IPRedundantLoadElimination.cpp
  IPStoreToLoadForwarding.cpp
  IPStoreSinking.cpp
  IPDeadStoreElimination.cpp
)
target_link_libraries(CanaryOptimization CanaryLLVMUtils)
