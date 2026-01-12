# IFDS Framework - Independent Analysis Library

# Framework core
set(IFDS_FRAMEWORK_SOURCES
  IDESolver.cpp
  IFDSDebugUtils.cpp
  #IFDSSolver.cpp          # moved to the include dir?
  #ParallelIFDSSolver.cpp  # moved to the include dir?
)

# Client analyses
set(IFDS_CLIENT_SOURCES
  Clients/IFDSTaintAnalysis.cpp
  Clients/IFDSReachingDefinitions.cpp
  Clients/IDEConstantPropagation.cpp
  Clients/IDETypeState.cpp
  Clients/IDEProtoAnalysis.cpp
)

set(IFDS_SOURCES ${IFDS_FRAMEWORK_SOURCES} ${IFDS_CLIENT_SOURCES})

add_library(IFDS STATIC ${IFDS_SOURCES})

# Allow headers to include .cpp template implementations
target_include_directories(IFDS PUBLIC ${CMAKE_SOURCE_DIR}/lib)

target_link_libraries(IFDS Annotation CanarySupport)
