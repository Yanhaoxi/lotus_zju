add_library(CanaryDebugInfo STATIC
    DebugInfoAnalysis.cpp
)
target_link_libraries(CanaryDebugInfo CanaryLLVMUtils)
