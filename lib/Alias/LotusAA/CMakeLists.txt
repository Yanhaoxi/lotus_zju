set(LOTUSAA_SOURCES
  MemObject.cpp
  ObjectLocator.cpp
  PointsToGraph.cpp
  IntraProceduralAnalysis.cpp
  Utilities.cpp
  CallGraphSolver.cpp
  FunctionSummary.cpp
  CallGraphState.cpp
  FunctionPointerResults.cpp
  LotusConfig.cpp
  InterProceduralPass.cpp
)

# Build as STATIC library for linking into executables
add_library(LotusAA STATIC ${LOTUSAA_SOURCES})

target_compile_options(LotusAA PRIVATE
  -fno-rtti
)

target_link_libraries(LotusAA PUBLIC
  LLVMAnalysis
)

# Set output directory
set_target_properties(LotusAA PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

install(TARGETS LotusAA
  ARCHIVE DESTINATION lib
)

