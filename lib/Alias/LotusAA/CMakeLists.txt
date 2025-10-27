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

add_library(LotusAA MODULE ${LOTUSAA_SOURCES})

target_compile_options(LotusAA PRIVATE
  -fno-rtti
)

target_link_libraries(LotusAA PRIVATE
  LLVMAnalysis
)

# Set output directory
set_target_properties(LotusAA PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

install(TARGETS LotusAA
  LIBRARY DESTINATION lib
)

