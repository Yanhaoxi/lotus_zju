# Sprattus: Static Program Analysis Toolkit Using Satisfiability
# Integrated into Lotus framework

# Sprattus library sources
set(SPRATTUS_SOURCES
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
    Core/SprattusPass.cpp
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

# Create Sprattus library
add_library(SprattusAnalysis STATIC ${SPRATTUS_SOURCES})

# Link LLVM libraries
llvm_map_components_to_libnames(sprattus_llvm_libs 
    asmparser bitreader bitwriter scalaropts transformutils)

target_link_libraries(SprattusAnalysis PUBLIC
    ${Z3_LIBRARIES}
    ${sprattus_llvm_libs}
)

# Installation
install(TARGETS SprattusAnalysis DESTINATION lib)
