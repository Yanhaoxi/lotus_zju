add_library(CanarySMT STATIC
        SMTConfigure.cpp
        SMTExpr.cpp
        SMTFactory.cpp
        SMTModel.cpp
        SMTObject.cpp
        SMTSolver.cpp
        SMTSampler.cpp
        CNF.cpp
        SATSolver.cpp
        SMTLIB2Solver.cpp
        Z3Expr.cpp
)

add_subdirectory(SymAbs)
