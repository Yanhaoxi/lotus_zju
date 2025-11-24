set(Concurrency_SOURCES
  ThreadAPI.cpp
  LockSetAnalysis.cpp
  MHPAnalysis.cpp
  ThreadFlowGraph.cpp
  MemUseDefAnalysis.cpp
  EscapeAnalysis.cpp
  HappensBeforeAnalysis.cpp
  StaticThreadSharingAnalysis.cpp
)
add_library(Concurrency STATIC ${Concurrency_SOURCES})
target_link_libraries(Concurrency CanaryLLVMUtils CanaryAliasAnalysisWrapper)

