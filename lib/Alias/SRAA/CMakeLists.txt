add_library(CanarySRAA STATIC
        RangeAnalysis.cpp
        StrictRelationAliasAnalysis.cpp
        vSSA.cpp
)
target_link_libraries(CanarySRAA PUBLIC CanaryLLVMUtils AliasSpecManager)
