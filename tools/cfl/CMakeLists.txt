# CFL tools umbrella


option(BUILD_CSR "Build indexing context-sensitive reachability solver" OFF)
if(BUILD_CSR)
    add_subdirectory(csr)
endif()

option(BUILD_GRASPAN "Build graspan" OFF)
if(BUILD_GRASPAN)
    add_subdirectory(graspan)
endif()


