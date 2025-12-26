# TODO: Following the code of clam to add the tests in tests/clam

cmake_minimum_required(VERSION 3.10)
find_package(LLVM REQUIRED CONFIG)
# GTest should already be found or downloaded by FindGTest.cmake in parent CMakeLists.txt
# Try find_package, but if targets already exist (from add_subdirectory), that's fine too
find_package(GTest QUIET)
if(NOT TARGET GTest::gtest AND NOT TARGET gtest)
    message(FATAL_ERROR "GTest not found. This should have been handled by FindGTest.cmake")
endif()

add_definitions(${LLVM_DEFINITIONS})

set(COMMON_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/utils ${CMAKE_CURRENT_SOURCE_DIR}/../include ${LLVM_INCLUDE_DIRS})
set(COMMON_LIBS ${LLVM_LIBRARIES} GTest::gtest GTest::gtest_main IFDS Annotation CanaryAliasAnalysisWrapper CanaryDyckAA LLVMIRReader CanaryPDG Concurrency SymAbs)

add_library(lotus_test_utils INTERFACE)
target_include_directories(lotus_test_utils INTERFACE ${COMMON_INCLUDES})
target_link_libraries(lotus_test_utils INTERFACE ${COMMON_LIBS})

function(add_lotus_test test_name source_file)
    add_executable(${test_name} ${source_file})
    set_target_properties(${test_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${LOTUS_TEST_BIN_DIR}
    )
    target_include_directories(${test_name} PRIVATE ${COMMON_INCLUDES})
    target_link_libraries(${test_name} lotus_test_utils)
endfunction()

function(add_lotus_pdg_test test_name source_file)
    add_executable(${test_name} ${source_file})
    set_target_properties(${test_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${LOTUS_TEST_BIN_DIR}
    )
    target_include_directories(${test_name} PRIVATE ${COMMON_INCLUDES})
    # PDG tests need additional LLVM analysis libraries
    target_link_libraries(${test_name} lotus_test_utils LLVMTransformUtils)
endfunction()

function(add_lotus_symabs_test test_name source_file)
    add_executable(${test_name} ${source_file})
    set_target_properties(${test_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${LOTUS_TEST_BIN_DIR}
    )
    target_include_directories(${test_name} PRIVATE ${COMMON_INCLUDES})
    # SymAbs tests need Z3 library and SymbolicAbstractionAnalysis for Z3APIExtension
    target_link_libraries(${test_name} lotus_test_utils ${Z3_LIBRARIES} SymbolicAbstractionAnalysis)
endfunction()

add_lotus_test(taint_analysis_test unit/TaintAnalysisTest.cpp)
add_lotus_test(ifds_solver_test unit/IFDSSolverTest.cpp)
add_lotus_test(egraph_test unit/EGraphTest.cpp)
add_lotus_pdg_test(pdg_slicing_test unit/pdg_slicing_test.cpp)
add_lotus_symabs_test(symabs_test unit/SymAbsTest.cpp)

# Transform pass tests
add_executable(lower_select_test unit/LowerSelectTest.cpp)
set_target_properties(lower_select_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${LOTUS_TEST_BIN_DIR}
)
target_include_directories(lower_select_test PRIVATE ${COMMON_INCLUDES})
target_link_libraries(lower_select_test lotus_test_utils CanaryTransform LLVMIRReader)

add_executable(remove_dead_block_test unit/RemoveDeadBlockTest.cpp)
set_target_properties(remove_dead_block_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${LOTUS_TEST_BIN_DIR}
)
target_include_directories(remove_dead_block_test PRIVATE ${COMMON_INCLUDES})
target_link_libraries(remove_dead_block_test lotus_test_utils CanaryTransform LLVMIRReader)

# Alias analysis test
add_executable(sparrow_aa_test unit/SparrowAATest.cpp)
set_target_properties(sparrow_aa_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${LOTUS_TEST_BIN_DIR}
)
target_include_directories(sparrow_aa_test PRIVATE ${COMMON_INCLUDES})
target_link_libraries(sparrow_aa_test lotus_test_utils Andersen LLVMIRReader)

# ICFG test
add_executable(icfg_test unit/ICFGTest.cpp)
set_target_properties(icfg_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${LOTUS_TEST_BIN_DIR}
)
target_include_directories(icfg_test PRIVATE ${COMMON_INCLUDES})
target_link_libraries(icfg_test lotus_test_utils CanaryICFG LLVMIRReader)

# DyckAA test
add_executable(dyck_aa_test unit/DyckAATest.cpp)
set_target_properties(dyck_aa_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${LOTUS_TEST_BIN_DIR}
)
target_include_directories(dyck_aa_test PRIVATE ${COMMON_INCLUDES})
target_link_libraries(dyck_aa_test lotus_test_utils CanaryDyckAA LLVMIRReader)

# AllocAA test
add_executable(alloc_aa_test unit/AllocAATest.cpp)
set_target_properties(alloc_aa_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${LOTUS_TEST_BIN_DIR}
)
target_include_directories(alloc_aa_test PRIVATE ${COMMON_INCLUDES})
target_link_libraries(alloc_aa_test lotus_test_utils CanaryAllocAA LLVMIRReader LLVMAnalysis)

# Mono dataflow test
add_executable(mono_test unit/MonoTest.cpp)
set_target_properties(mono_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${LOTUS_TEST_BIN_DIR}
)
target_include_directories(mono_test PRIVATE ${COMMON_INCLUDES})
target_link_libraries(mono_test lotus_test_utils MONODataFlow LLVMIRReader)

# WPDS test
add_executable(wpds_test unit/WPDSTest.cpp)
set_target_properties(wpds_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${LOTUS_TEST_BIN_DIR}
)
target_include_directories(wpds_test PRIVATE ${COMMON_INCLUDES})
target_link_libraries(wpds_test lotus_test_utils WPDSDataFlow)

# FPSolve tests
add_executable(fpsolve_test unit/FPSolveTest.cpp)
set_target_properties(fpsolve_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${LOTUS_TEST_BIN_DIR}
)
target_include_directories(fpsolve_test PRIVATE ${COMMON_INCLUDES})
target_link_libraries(fpsolve_test GTest::gtest GTest::gtest_main FPSolve)
# MHP test needs additional LLVM analysis libraries
add_executable(mhp_analysis_test unit/MHPAnalysisTest.cpp)
set_target_properties(mhp_analysis_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${LOTUS_TEST_BIN_DIR}
)
target_include_directories(mhp_analysis_test PRIVATE ${COMMON_INCLUDES})
target_link_libraries(mhp_analysis_test lotus_test_utils LLVMAnalysis)

# AserPTA test
add_executable(aser_pta_test unit/AserPTATest.cpp)
set_target_properties(aser_pta_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${LOTUS_TEST_BIN_DIR}
)
target_include_directories(aser_pta_test PRIVATE ${COMMON_INCLUDES})
target_link_libraries(aser_pta_test lotus_test_utils AserPTA LLVMIRReader)

enable_testing()
add_test(NAME TaintAnalysisTest COMMAND ${LOTUS_TEST_BIN_DIR}/taint_analysis_test)
add_test(NAME IFDSSolverTest COMMAND ${LOTUS_TEST_BIN_DIR}/ifds_solver_test)
add_test(NAME EGraphTest COMMAND ${LOTUS_TEST_BIN_DIR}/egraph_test)
add_test(NAME PDGSlicingTest COMMAND ${LOTUS_TEST_BIN_DIR}/pdg_slicing_test)
add_test(NAME MHPAnalysisTest COMMAND ${LOTUS_TEST_BIN_DIR}/mhp_analysis_test)
add_test(NAME FPSolveTest COMMAND ${LOTUS_TEST_BIN_DIR}/fpsolve_test)
add_test(NAME SymAbsTest COMMAND ${LOTUS_TEST_BIN_DIR}/symabs_test)
add_test(NAME LowerSelectTest COMMAND ${LOTUS_TEST_BIN_DIR}/lower_select_test)
add_test(NAME RemoveDeadBlockTest COMMAND ${LOTUS_TEST_BIN_DIR}/remove_dead_block_test)
add_test(NAME SparrowAATest COMMAND ${LOTUS_TEST_BIN_DIR}/sparrow_aa_test)
add_test(NAME ICFGTest COMMAND ${LOTUS_TEST_BIN_DIR}/icfg_test)
add_test(NAME DyckAATest COMMAND ${LOTUS_TEST_BIN_DIR}/dyck_aa_test)
add_test(NAME AllocAATest COMMAND ${LOTUS_TEST_BIN_DIR}/alloc_aa_test)
add_test(NAME MonoTest COMMAND ${LOTUS_TEST_BIN_DIR}/mono_test)
add_test(NAME WPDSTest COMMAND ${LOTUS_TEST_BIN_DIR}/wpds_test)
add_test(NAME AserPTATest COMMAND ${LOTUS_TEST_BIN_DIR}/aser_pta_test)
set_tests_properties(TaintAnalysisTest IFDSSolverTest EGraphTest PDGSlicingTest MHPAnalysisTest FPSolveTest SymAbsTest LowerSelectTest RemoveDeadBlockTest SparrowAATest ICFGTest DyckAATest AllocAATest MonoTest WPDSTest AserPTATest PROPERTIES TIMEOUT 300 WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

add_custom_target(run_all_tests COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure DEPENDS taint_analysis_test ifds_solver_test egraph_test pdg_slicing_test mhp_analysis_test fpsolve_test symabs_test lower_select_test remove_dead_block_test sparrow_aa_test icfg_test dyck_aa_test alloc_aa_test mono_test wpds_test aser_pta_test)
add_custom_target(run_unit_tests COMMAND ${CMAKE_CTEST_COMMAND} -R "Test$" --output-on-failure DEPENDS taint_analysis_test ifds_solver_test egraph_test pdg_slicing_test mhp_analysis_test fpsolve_test symabs_test lower_select_test remove_dead_block_test sparrow_aa_test icfg_test dyck_aa_test alloc_aa_test mono_test wpds_test aser_pta_test)

