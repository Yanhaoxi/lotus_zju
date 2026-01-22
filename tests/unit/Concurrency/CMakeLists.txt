add_lotus_test(mhp_analysis_test MHPAnalysisTest.cpp)
add_lotus_test(lockset_analysis_test LockSetAnalysisTest.cpp)

message(STATUS "Before adding simple_atomic_test_test")
add_lotus_test(simple_atomic_test_test simple_atomic_test.cpp)
message(STATUS "After adding simple_atomic_test_test")