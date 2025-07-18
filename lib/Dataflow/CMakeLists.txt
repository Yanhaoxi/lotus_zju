set(WPDS_SOURCES
  Mono/DataFlowAnalysis.cpp
  Mono/DataFlowEngine.cpp
  Mono/DataFlowResult.cpp
  WPDS/DataFlowFacts.cpp
  WPDS/GenKillTransformer.cpp
  WPDS/InterProceduralDataFlowEngine.cpp
)
add_library(DataFlow STATIC ${WPDS_SOURCES}) 

# Null pointer analsyis over DyckVFG
set(CanaryNullPointer_SOURCES
    NullPointer/LocalNullCheckAnalysis.cpp
    NullPointer/NullCheckAnalysis.cpp
    NullPointer/NullEquivalenceAnalysis.cpp
    NullPointer/NullFlowAnalysis.cpp
    NullPointer/ContextSensitiveLocalNullCheckAnalysis.cpp
    NullPointer/ContextSensitiveNullCheckAnalysis.cpp
    NullPointer/ContextSensitiveNullFlowAnalysis.cpp
    NullPointer/AliasAnalysisAdapter.cpp
)
add_library(CanaryNullPointer STATIC ${CanaryNullPointer_SOURCES}) 

# Global value flow analsyis over DyckVFG
set(GVFA_SOURCES
    GVFA/GlobalValueFlowAnalysis.cpp
)
add_library(GVFA STATIC ${GVFA_SOURCES})
target_link_libraries(GVFA CanaryDyckAA CanarySupport CanaryTaint) 

# Configuration Tainter analysis
set(CONFTAINTER_SOURCES
    ConfTainter/ConfTainter.cpp
)
add_library(ConfTainter STATIC ${CONFTAINTER_SOURCES})
target_link_libraries(ConfTainter CanarySupport)

# based on LLVM 3.8; to be upgraded
# set(RANGE_ANALYSIS_SOURCES
#    Range/vSSA.cpp
#    Range/RangeAnalysis.cpp
#)





