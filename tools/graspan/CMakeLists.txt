# Graspan - CFL Reachability Tool
#
# Disk-based parallel graph system for computing dynamic transitive closures


# Define macros to avoid boost::throw_exception issues and provide our own implementation
add_definitions(-DBOOST_NO_EXCEPTIONS -DBOOST_EXCEPTION_DISABLE)

# Include Boost headers first
include_directories(${Boost_INCLUDE_DIRS})

# Source files for Graspan
set(GRASPAN_SOURCES
    src/dispatcher/graspan.cpp
    src/datastructures/computationset.h
    src/datastructures/context.cpp
    src/datastructures/context.h
    src/datastructures/DDM.cpp
    src/datastructures/DDM.h
    src/datastructures/loadedvertexinterval.cpp
    src/datastructures/loadedvertexinterval.h
    src/datastructures/loader.cpp
    src/datastructures/loader.h
    src/datastructures/minset.cpp
    src/datastructures/minset.h
    src/datastructures/partition.cpp
    src/datastructures/partition.h
    src/datastructures/vertex.cpp
    src/datastructures/vertex.h
    src/datastructures/vit.cpp
    src/datastructures/vit.h
    src/edgecomp/compute.cpp
    src/edgecomp/compute.h
    src/edgecomp/edgemerger.cpp
    src/edgecomp/edgemerger.h
    src/edgecomp/engine.cpp
    src/edgecomp/engine.h
    src/edgecomp/grammar.cpp
    src/edgecomp/grammar.h
    src/edgecomp/repart.cpp
    src/edgecomp/repart.h
    src/preproc/preproc.cpp
    src/preproc/preproc.h
    src/preproc/preproc_new.cpp
    src/preproc/preproc_new.h
    src/preproc/run_pre.cpp
    src/preproc/run_pre.h
    src/utilities/timer.cpp
    src/utilities/timer.h
    src/utilities/boost_throw_exception.h
    src/utilities/globalDefinitions.hpp
)

# Create Graspan executable
add_executable(graspan ${GRASPAN_SOURCES})

# Link libraries (using project conventions)
target_link_libraries(graspan
    Boost::system
    Boost::thread
    pthread
)
