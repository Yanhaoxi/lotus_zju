set(CanaryNullPointer_SOURCES
  LocalNullCheckAnalysis.cpp
  NullCheckAnalysis.cpp
  NullEquivalenceAnalysis.cpp
  NullFlowAnalysis.cpp
  ContextSensitiveLocalNullCheckAnalysis.cpp
  ContextSensitiveNullCheckAnalysis.cpp
  ContextSensitiveNullFlowAnalysis.cpp
  AliasAnalysisAdapter.cpp
)
add_library(CanaryNullPointer STATIC ${CanaryNullPointer_SOURCES})
target_link_libraries(CanaryNullPointer CanaryLLVMUtils CanaryDyckAA)