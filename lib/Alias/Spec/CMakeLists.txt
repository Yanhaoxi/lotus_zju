add_library(AliasSpecManager
  AliasSpecManager.cpp
  AliasSpecManagerUtils.cpp
  AliasSpecManagerQueries.cpp
)

# Link against LLVMDemangle since AliasSpecManager.cpp uses llvm::demangle()
target_link_libraries(AliasSpecManager PUBLIC
  LotusAnnotation
  LLVMDemangle
)
