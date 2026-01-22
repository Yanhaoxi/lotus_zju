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
