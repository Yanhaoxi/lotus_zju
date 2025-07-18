# Find out what libraries are needed by LLVM
set(LLVM_LINK_COMPONENTS
  LLVMIRReader
  LLVMAnalysis
)

add_executable(conftainter conftainter.cpp)

target_link_libraries(conftainter PRIVATE
  ConfTainter
  ${LLVM_LINK_COMPONENTS}
) 