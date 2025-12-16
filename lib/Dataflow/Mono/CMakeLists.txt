# Mono-based dataflow analysis implementation

# Framework core
set(MONO_FRAMEWORK_SOURCES
  DataFlowAnalysis.cpp
  DataFlowEngine.cpp
  DataFlowResult.cpp
)

# Client analyses
set(MONO_CLIENT_SOURCES
  Clients/LiveVariablesAnalysis.cpp
  Clients/ReachableAnalysis.cpp
)

set(MONO_SOURCES ${MONO_FRAMEWORK_SOURCES} ${MONO_CLIENT_SOURCES})

add_library(MONODataFlow STATIC ${MONO_SOURCES})

  
