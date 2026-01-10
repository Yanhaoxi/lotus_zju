add_library(SMTSampler STATIC
    QuickSampler.cpp
    IntervalSampler.cpp
    RegionSampler.cpp
    PolySampler/HitAndRun.cpp
    PolySampler/DikinWalk.cpp
    PolySampler/CoordinateWalk.cpp
    PolySampler/PolySampler.cpp
    SMTSampler.cpp
)

target_link_libraries(SMTSampler
    PRIVATE SymAbs
)
