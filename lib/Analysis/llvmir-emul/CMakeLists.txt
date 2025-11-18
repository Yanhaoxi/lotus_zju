# CMakeLists.txt for llvmir-emul library

set(LLVMIR_EMUL_SOURCES
    llvmir_emul.cpp
    emulator/binary_instructions.cpp
    emulator/comparison_instructions.cpp
    emulator/constant_handling.cpp
    emulator/conversion_instructions.cpp
    emulator/execution_context.cpp
    emulator/helper_functions.cpp
    emulator/instruction_visitors_binary.cpp
    emulator/instruction_visitors_simple.cpp
    emulator/instruction_visitors_memory.cpp
    emulator/instruction_visitors_shift.cpp
    emulator/instruction_visitors_terminator.cpp
    # Miri components
    BugDetection.cpp
    MemoryModel.cpp
    MiriEmulator.cpp
    MemorySafetyChecker.cpp
    DivisionByZeroChecker.cpp
    IntegerOverflowChecker.cpp
    InvalidShiftChecker.cpp
    NullPointerChecker.cpp
    UninitializedMemoryChecker.cpp
    BugCheckerFactory.cpp
)

add_library(llvmir-emul STATIC ${LLVMIR_EMUL_SOURCES})

# Link LLVM CodeGen library for IntrinsicLowering
llvm_map_components_to_libnames(llvm_codegen_libs codegen)
target_link_libraries(llvmir-emul
    ${llvm_codegen_libs}
)
