add_library(OriginAA STATIC
    CallGraph.cpp
    ChannelSemantics.cpp
    Flags.cpp
    KCallsitePointerAnalysis.cpp
    OriginPointerAnalysis.cpp
    PointerAnalysis.cpp
    PointerAnalysisPass.cpp
)

# Set C++ standard to 17 for inline variables
# set_target_properties(OriginAA PROPERTIES
#    CXX_STANDARD 17
#    CXX_STANDARD_REQUIRED ON
#)