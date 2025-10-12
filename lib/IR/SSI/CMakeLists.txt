# Static Single Information (SSI) IR CMake configuration

add_library(CanarySSI STATIC
  SSIPass.cpp
)

target_include_directories(CanarySSI PUBLIC
  ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(CanarySSI PUBLIC
  LLVMCore
  LLVMSupport
)


