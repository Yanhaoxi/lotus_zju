# Find out what libraries are needed by LLVM
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