# IFDS Framework - Independent Analysis Library

# Framework core
set(IFDS_FRAMEWORK_SOURCES
  IDESolver.cpp
  IFDSDebugUtils.cpp
  IFDSSolver.cpp
  ParallelIFDSSolver.cpp
)

# Client analyses
set(IFDS_CLIENT_SOURCES
  Clients/IFDSTaintAnalysis.cpp
  Clients/IFDSReachingDefinitions.cpp
  Clients/IDEConstantPropagation.cpp
  Clients/IDETypeState.cpp
)

set(IFDS_SOURCES ${IFDS_FRAMEWORK_SOURCES} ${IFDS_CLIENT_SOURCES})

add_library(IFDS STATIC ${IFDS_SOURCES})

# Allow headers to include .cpp template implementations
target_include_directories(IFDS PUBLIC ${CMAKE_SOURCE_DIR}/lib)

target_link_libraries(IFDS Annotation CanarySupport)

