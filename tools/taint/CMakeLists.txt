# Find out what libraries are needed by LLVM
llvm_map_components_to_libnames(LLVM_LINK_COMPONENTS
  IRReader
)

add_executable(taint-analyzer taint-analyzer.cpp)
target_link_libraries(taint-analyzer PRIVATE
        CanaryTaint
        ${LLVM_LINK_COMPONENTS}
        z ncurses pthread dl
) 