# External TBB setup
# (NOT used for now)
find_package(TBB)

if(NOT TBB_FOUND)
    message(STATUS "TBB not found, downloading and building it...")

    ExternalProject_Add(
        tbb
        GIT_REPOSITORY https://github.com/oneapi-src/oneTBB.git
        GIT_TAG v2021.13.0
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/tbb_install -DCMAKE_INSTALL_LIBDIR=lib -DTBB_TEST=OFF
        BUILD_COMMAND ${CMAKE_COMMAND} --build . --target install
        INSTALL_COMMAND ""
        BUILD_IN_SOURCE 1
        PREFIX ${CMAKE_BINARY_DIR}/_deps/tbb
    )

    set(TBB_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/tbb_install/include")
    set(TBB_LIB "${CMAKE_BINARY_DIR}/tbb_install/lib/libtbb${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(TBB_MALLOC_LIB "${CMAKE_BINARY_DIR}/tbb_install/lib/libtbbmalloc${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(TBB_MALLOC_PROXY_LIB "${CMAKE_BINARY_DIR}/tbb_install/lib/libtbbmalloc_proxy${CMAKE_SHARED_LIBRARY_SUFFIX}")

    include_directories(${TBB_INCLUDE_DIRS})
    set(TBB_LIBS ${TBB_LIB} ${TBB_MALLOC_LIB} ${TBB_MALLOC_PROXY_LIB})

    add_custom_target(libTBB)
    add_dependencies(libTBB tbb)
else()
    set(TBB_LIBS TBB::tbb TBB::tbbmalloc TBB::tbbmalloc_proxy)
endif()


if(TARGET libTBB)
    add_dependencies(${LIB_NAME} libTBB)
endif()