# Examples CMakeLists.txt

set(EXAMPLES
  ModelCheckingExample.cpp
  RapidJSONExample.cpp
  AndersenExample.cpp
  PDGExample.cpp
)

set(EXAMPLES_NAMES
  ModelCheckingExample
  RapidJSONExample
  AndersenExample
  PDGExample
)

set(EXAMPLES_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)

# Get LLVM components needed for examples that don't get LLVM through dependencies
llvm_map_components_to_libnames(llvm_libs irreader analysis)

# Add executables and include directories
foreach(idx RANGE 0 4)
  add_executable(${EXAMPLES_NAMES[idx]} ${EXAMPLES[idx]})
  target_include_directories(${EXAMPLES_NAMES[idx]} PUBLIC ${EXAMPLES_INCLUDE_DIR})
endforeach()

# AndersenExample needs AndersenStatic and LLVM libs
 target_link_libraries(AndersenExample PRIVATE AndersenStatic ${llvm_libs})

# PDGExample needs CanaryPDG and LLVM libs
 target_link_libraries(PDGExample PRIVATE CanaryPDG ${llvm_libs}) 