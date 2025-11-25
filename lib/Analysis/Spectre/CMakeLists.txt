set(Spectre_SOURCES
  CacheSpecuAnalysis.cpp
)
add_library(Spectre STATIC ${Spectre_SOURCES})
target_link_libraries(Spectre CanaryLLVMUtils)
