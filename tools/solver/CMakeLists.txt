set(LLVM_LINK_COMPONENTS
        LLVMSupport
)

add_executable(owl owl.cpp)
target_link_libraries(owl PRIVATE
        CanarySMT
        ${LLVM_LINK_COMPONENTS}
        ${Z3_LIBRARIES}
)

# TODO (mabye do not create independent binaries for slot and staub?)
# slot
# staub