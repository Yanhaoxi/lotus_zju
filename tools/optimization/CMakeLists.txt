llvm_map_components_to_libnames(LLVM_LINK_COMPONENTS
  Core
  IRReader
  Passes
)

add_executable(lotus-opt lotus-opt.cpp)
target_link_libraries(lotus-opt PRIVATE
  CanaryOptimization
  SeaDsaAnalysis
  ${LLVM_LINK_COMPONENTS}
)
