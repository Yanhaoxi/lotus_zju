# SeaHorn configuration

# Configure GitSHA1.cc
set(GIT_SHA1 "unknown")
find_package(Git)
if(GIT_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_SHA1
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
endif()
configure_file(
  "${CMAKE_SOURCE_DIR}/lib/Verification/seahorn/Support/GitSHA1.cc.in"
  "${CMAKE_SOURCE_DIR}/lib/Verification/seahorn/Support/GitSHA1.cc"
  @ONLY
)

# SeaHorn configuration
set(SeaHorn_VERSION_INFO "dev")
set(HAVE_CLAM ${HAVE_CLAM})
set(HAVE_DSA ON)
set(HAVE_LLVM_SEAHORN OFF)
configure_file(
  ${CMAKE_SOURCE_DIR}/include/Verification/seahorn/config.h.cmake
  ${CMAKE_BINARY_DIR}/include/Verification/seahorn/config.h
)
include_directories(${CMAKE_BINARY_DIR}/include ${CMAKE_BINARY_DIR}/include/Verification)

