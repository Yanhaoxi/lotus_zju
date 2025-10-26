# FPSolve - Fixed-Point Solver Library
# Based on FPsolve-master

set(FPSolve_SRCS
  # Data structures
  Var.cpp
  
  # Semirings
  BoolSemiring.cpp
  TropicalSemiring.cpp
  FreeSemiring.cpp
  FreeStructure.cpp
  CommutativeRExp.cpp
  PrefixSemiring.cpp
  WhySemiring.cpp
  
  # Polynomials
  CommutativePolynomial.cpp
  CommutativeMonomial.cpp
  
  # Parser
  Parser.cpp
  
  # Examples and tools (optional, can be commented out if not needed)
  # MainExample.cpp
  # GrammarChecker.cpp
)

# Check for Boost (required for Parser)
find_package(Boost COMPONENTS program_options QUIET)
if(Boost_FOUND)
  message(STATUS "Found Boost: ${Boost_VERSION}")
else()
  message(STATUS "Boost not found - some features may not be available")
  message(STATUS "Parser requires Boost.Spirit (header-only)")
endif()

# Check for libfa (optional)
find_package(PkgConfig)
if(PkgConfig_FOUND)
  pkg_check_modules(LIBFA libfa)
  if(LIBFA_FOUND)
    message(STATUS "Found libfa: ${LIBFA_LIBRARIES}")
    add_definitions(-DHAVE_LIBFA)
    list(APPEND FPSolve_SRCS LossyFiniteAutomaton.cpp)
  else()
    message(STATUS "libfa not found - LossyFiniteAutomaton will not be available")
    message(STATUS "To install: sudo apt-get install libfa-dev (Ubuntu)")
    message(STATUS "Or build from: http://augeas.net/libfa/")
  endif()
endif()

add_library(FPSolve STATIC ${FPSolve_SRCS})

# Enable RTTI for FreeStructure
set_target_properties(FPSolve PROPERTIES
  CXX_STANDARD_REQUIRED ON
)

# Remove -fno-rtti if it's set
target_compile_options(FPSolve PRIVATE -frtti)

target_include_directories(FPSolve PUBLIC
  ${LIBFA_INCLUDE_DIRS}
  ${Boost_INCLUDE_DIRS}
)

if(LIBFA_FOUND)
  target_link_libraries(FPSolve ${LIBFA_LIBRARIES})
endif()

if(Boost_FOUND)
  target_link_libraries(FPSolve ${Boost_LIBRARIES})
endif()

# Install headers
#install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/Solvers/FPSolve
#  DESTINATION include/Solvers
#  FILES_MATCHING PATTERN "*.h"
#)

# Install library
#install(TARGETS FPSolve
#  ARCHIVE DESTINATION lib
#  LIBRARY DESTINATION lib
#)
