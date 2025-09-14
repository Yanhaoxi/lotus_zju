set(SPARTA_SOURCES
  LLVMValueDomain.cpp
  LLVMAbstractState.cpp
  LLVMTransferFunction.cpp
  LLVMFixpointIterator.cpp
  LLVMAbstractInterpreter.cpp
  BoostExceptionStub.cpp
)

add_library(SpartaLLVM STATIC ${SPARTA_SOURCES})

# Link against LLVM libraries
target_link_libraries(SpartaLLVM
  LLVMSupport
  LLVMIRReader
  LLVMAnalysis
)

# Include directories
target_include_directories(SpartaLLVM PUBLIC
  ${CMAKE_SOURCE_DIR}/include
)

# Set C++ standard
set_property(TARGET SpartaLLVM PROPERTY CXX_STANDARD 17)
set_property(TARGET SpartaLLVM PROPERTY CXX_STANDARD_REQUIRED ON)
