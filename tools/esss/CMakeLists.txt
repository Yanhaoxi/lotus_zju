set(LLVM_LINK_COMPONENTS
  LLVMBitWriter
  LLVMIRReader
)

add_executable(esss esss.cpp)

target_link_libraries(esss
  PRIVATE
  CanaryESSS
  CanarySupport
  ${LLVM_LINK_COMPONENTS}
  pthread dl
)