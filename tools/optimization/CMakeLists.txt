llvm_map_components_to_libnames(LLVM_LINK_COMPONENTS
  Core
  IRReader
  Passes
)

add_executable(lotus-opt lotus-opt.cpp)
if(APPLE)
  # On macOS, use -force_load to ensure all symbols are included
  target_link_options(lotus-opt PRIVATE
    -Wl,-force_load,$<TARGET_FILE:CanaryOptimization>
  )
  target_link_libraries(lotus-opt PRIVATE
    CanaryMemorySSA
    CanaryLLVMUtils
    SeaDsaAnalysis
    ${LLVM_LINK_COMPONENTS}
  )
else()
  # On Linux, use --whole-archive
  target_link_options(lotus-opt PRIVATE
    -Wl,--whole-archive
  )
  target_link_libraries(lotus-opt PRIVATE
    CanaryOptimization
  )
  target_link_options(lotus-opt PRIVATE
    -Wl,--no-whole-archive
  )
  target_link_libraries(lotus-opt PRIVATE
    CanaryMemorySSA
    CanaryLLVMUtils
    SeaDsaAnalysis
    ${LLVM_LINK_COMPONENTS}
  )
endif()
