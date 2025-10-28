# Alias analysis tools

llvm_map_components_to_libnames(LLVM_LINK_COMPONENTS
  Coroutines
  Target
)

# AserPTA Pointer Analysis Tool
add_executable(aser_pta aser_pta.cpp)
target_link_libraries(aser_pta PRIVATE
  AserPTA
  ${LLVM_LINK_COMPONENTS}
)

# Sea-DSA Memory Graph Analysis Tool
add_executable(sea-dsa-dg sea-dsa-dg.cpp)
target_link_libraries(sea-dsa-dg PRIVATE
  SeaDsaAnalysis
)

# Sea-DSA Advanced Analysis Tool
add_executable(seadsa-tool seadsa-tool.cpp)
target_link_libraries(seadsa-tool PRIVATE
  SeaDsaAnalysis
)

# FPA (Flow-sensitive Pointer Analysis) Tool
add_executable(fpa fpa.cpp)
target_link_libraries(fpa PRIVATE
  CanaryFPA
  ${LLVM_LINK_COMPONENTS}
)

# LotusAA Pointer Analysis Tool
add_executable(lotus_aa lotus_aa.cpp)
target_link_libraries(lotus_aa PRIVATE
  LotusAA
  ${LLVM_LINK_COMPONENTS}
)

# Andersen's Pointer Analysis Tool
add_executable(ander_aa ander_aa.cpp)
target_link_libraries(ander_aa PRIVATE
  Andersen
  ${LLVM_LINK_COMPONENTS}
)

# Dynamic Alias Analysis tools (only built if BUILD_DYNAA is enabled)
if(BUILD_DYNAA)
  add_subdirectory(dynaa)
endif()