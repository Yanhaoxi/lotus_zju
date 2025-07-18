set(LLVM_LINK_COMPONENTS
        #LLVMDemangle
        LLVMSupport
)

add_executable(owl owl.cpp)
target_link_libraries(owl PRIVATE
        CanarySMT
        ${LLVM_LINK_COMPONENTS}
        ${Z3_LIBRARIES}
        z ncurses pthread dl
)