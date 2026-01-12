# IFDS/IDE Unit Tests

# IFDS Framework Core Test (migrated from phasar-development)
add_lotus_test(ifds_framework_core_test IFDSFrameworkCoreTest.cpp)

# IFDS Solver Test
add_lotus_test(ifds_solver_test IFDSSolverTest.cpp)

# Taint Analysis Test
add_lotus_test(taint_analysis_test TaintAnalysisTest.cpp)
