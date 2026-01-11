# NPA sources 
set(NPA_SOURCES
  Domains/BitVectorDomain.cpp
  Domains/GenKillDomain.cpp
  Domains/TaintTransferDomain.cpp
  Engines/BitVectorSolver.cpp
  Clients/ReachingDefinitions.cpp
  Clients/ReachableBlocks.cpp
  Clients/InterproceduralRD.cpp
  Clients/InterproceduralTaint.cpp
)

add_library(NPADataFlow STATIC ${NPA_SOURCES})
