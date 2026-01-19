# Mono-based dataflow analysis implementation

# Client analyses
set(MONO_CLIENT_SOURCES
  Clients/IntraMonoConstantPropagation.cpp
  Clients/IntraMonoUninitVariables.cpp
  Clients/InterMonoTaintAnalysis.cpp
  Clients/LiveVariablesAnalysis.cpp
  Clients/ReachableAnalysis.cpp
)

set(MONO_SOURCES ${MONO_CLIENT_SOURCES})

add_library(MONODataFlow STATIC ${MONO_SOURCES})

  
