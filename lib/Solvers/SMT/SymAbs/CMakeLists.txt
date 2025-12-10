add_library(SymAbs STATIC
        LinearExpression.cpp
        Octagon.cpp
        Congruence.cpp
        Affine.cpp
        Polyhedron.cpp
        Polynomial.cpp
        SMTLibOptimizer.cpp
        SMTOptimization.cpp
)

target_link_libraries(SymAbs
        CanarySMT
        SymbolicAbstractionAnalysis
)
