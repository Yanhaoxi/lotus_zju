# SymbolicAbstraction: Static Program Analysis Toolkit Using Satisfiability
# Integrated into Lotus framework
#
# This is a PROGRAM-LEVEL analysis framework for LLVM IR with abstract interpretation.
# It provides fixpoint engines, abstract domains, and LLVM pass integration.
#
# ⚠️ This is NOT the same as SymAbs (lib/Solvers/SMT/SymAbs), which provides
# FORMULA-LEVEL abstraction algorithms for SMT formulas.
#
# See README.md for details on the distinction.

# SymbolicAbstraction library sources
set(SYMBOLIC_ABSTRACTION_SOURCES
    # Analyzers
    Analyzers/Analyzer.cpp
    Analyzers/UnilateralAnalyzer.cpp
    Analyzers/BilateralAnalyzer.cpp
    Analyzers/OMTAnalyzer.cpp
    # Core components
    Core/Fragment.cpp
    Core/FragmentDecomposition.cpp
    Core/Expression.cpp
    Core/ConcreteState.cpp
    Core/InstructionSemantics.cpp
    Core/MemoryModel.cpp
    Core/FloatingPointModel.cpp
    Core/FunctionContext.cpp
    Core/ModuleContext.cpp
    Core/RepresentedValue.cpp
    Core/ValueMapping.cpp
    Core/DomainConstructor.cpp
    Core/SymbolicAbstractionPass.cpp
    Core/Checks.cpp
    Core/ResultStore.cpp
    Core/ParamStrategy.cpp
    # Utilities
    Utils/Utils.cpp
    Utils/Config.cpp
    Utils/PrettyPrinter.cpp
    Utils/Reporting.cpp
    Utils/Z3APIExtension.cpp
    # Abstract Domains
    Domains/NumRels.cpp
    Domains/SimpleConstProp.cpp
    Domains/BitMask.cpp
    Domains/Affine.cpp
    Domains/Predicates.cpp
    Domains/Intervals.cpp
    Domains/MemRange.cpp
    Domains/MemRegions.cpp
    Domains/Boolean.cpp
    Domains/Congruence.cpp
    Domains/Zones.cpp
    Domains/Octagon.cpp
)

# Create SymbolicAbstraction library
add_library(SymbolicAbstractionAnalysis STATIC ${SYMBOLIC_ABSTRACTION_SOURCES})

# Link LLVM libraries
llvm_map_components_to_libnames(symbolic_abstraction_llvm_libs 
    asmparser bitreader bitwriter scalaropts transformutils)

target_link_libraries(SymbolicAbstractionAnalysis PUBLIC
    ${Z3_LIBRARIES}
    ${symbolic_abstraction_llvm_libs}
)

# Installation
install(TARGETS SymbolicAbstractionAnalysis DESTINATION lib)
