set(LLVM_LINK_COMPONENTS
        LLVMPasses
)

add_executable(kint kint.cpp)

target_link_libraries(kint PRIVATE
        CanarySupport
        ${LLVM_LINK_COMPONENTS}
        ${Z3_LIBRARIES} pthread dl
)

