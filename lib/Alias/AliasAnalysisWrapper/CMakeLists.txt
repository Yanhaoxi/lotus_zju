
llvm_map_components_to_libnames(cli_utils_llvm_libs Core IRReader Support)

# CLI utilities library for alias analysis command-line tools
add_library(CanaryAliasCLIUtils STATIC CLIUtils.cpp)
target_link_libraries(CanaryAliasCLIUtils PUBLIC AliasSpecManager ${cli_utils_llvm_libs})

# Alias Analysis Wrapper library
add_library(CanaryAliasAnalysisWrapper STATIC
    AliasAnalysisWrapperCore.cpp
    AliasAnalysisWrapperQueries.cpp
    AliasAnalysisWrapperBackend.cpp
    AliasAnalysisFactory.cpp
    AliasAnalysisConfigParser.cpp
)

target_link_libraries(CanaryAliasAnalysisWrapper
    PUBLIC
        Andersen
        CanaryDyckAA
        CFLAA
        SeaDsaAnalysis
        CanaryAllocAA
        CanaryUnderApproxAA
        FSCSPointerAnalysis
        TPATransforms
        TPAUtil
)
