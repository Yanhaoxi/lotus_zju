# WPDS-based interprocedural dataflow analysis implementation

# Framework core
set(WPDS_FRAMEWORK_SOURCES
  DataFlowFacts.cpp
  GenKillTransformer.cpp
  InterProceduralDataFlowEngine.cpp
)

# Client analyses
set(WPDS_CLIENT_SOURCES
  Clients/WPDSUninitializedVariables.cpp
  Clients/WPDSConstantPropagation.cpp
  Clients/WPDSLivenessAnalysis.cpp
  Clients/WPDSTaintAnalysis.cpp
)

set(WPDS_SOURCES ${WPDS_FRAMEWORK_SOURCES} ${WPDS_CLIENT_SOURCES})

add_library(WPDSDataFlow STATIC ${WPDS_SOURCES})
