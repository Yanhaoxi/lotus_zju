add_library(CanaryAliasBDD STATIC
  BDDPtsSet.cpp
)

target_link_libraries(CanaryAliasBDD
  PUBLIC CanaryCUDD
)
