# DyckAA-based null pointer analysis, global value-flow analysis.

llvm_map_components_to_libnames(LLVM_LINK_COMPONENTS
  Coroutines
  Target
)

add_executable(canary canary.cpp)
target_link_libraries(canary PRIVATE
  CanaryNullPointer 
  CanaryDyckAA 
  CanaryTransform 
  CanarySupport
  ${LLVM_LINK_COMPONENTS}
)

add_executable(gvfa-tool gvfa-tool.cpp)
target_link_libraries(gvfa-tool PRIVATE
  GVFA
  ${LLVM_LINK_COMPONENTS}
) 