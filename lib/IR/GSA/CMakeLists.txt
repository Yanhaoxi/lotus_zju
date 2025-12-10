add_library(CanaryGSA STATIC
  ControlDependenceAnalysis.cpp
  GateAnalysis.cpp
)

target_link_libraries(CanaryGSA PUBLIC
  LLVMCore
  LLVMAnalysis
  LLVMSupport
)

