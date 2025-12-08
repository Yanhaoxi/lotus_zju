set(CanaryTypeHirarchy_SOURCES
  TypeHierarchyAnalysis.cpp
  DIBasedTypeHierarchy.cpp
  DIBasedTypeHierarchyData.cpp
  LLVMVFTable.cpp
  LLVMVFTableData.cpp
)
add_library(CanaryTypeHirarchy STATIC ${CanaryTypeHirarchy_SOURCES})
target_link_libraries(CanaryTypeHirarchy CanaryLLVMUtils)

# Suppress deprecation warnings from third-party libraries (spdlog/fmt)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(CanaryTypeHirarchy PRIVATE -Wno-deprecated-declarations)
endif()
