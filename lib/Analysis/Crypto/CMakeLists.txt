set(CryptoVerify_SOURCES
  ctllvm.cpp
  ctllvm_analysis.cpp
  ctllvm_inlining.cpp
)
add_library(CryptoVerify STATIC ${CryptoVerify_SOURCES})
