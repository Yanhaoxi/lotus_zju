add_library(CanaryOptimization STATIC
  AInliner.cpp
  ModuleOptimizer.cpp
  SWPrefetching.cpp
  LICM.cpp
)
target_link_libraries(CanaryOptimization CanaryLLVMUtils)
