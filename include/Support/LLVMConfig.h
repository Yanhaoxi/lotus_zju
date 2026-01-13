#pragma once

#include "Support/LLVMVersion.h"

// Opaque pointer support
#if LLVM_VERSION_MAJOR >= 15
  #define LOTUS_ENABLE_OPAQUE_POINTERS 1
#else
  #define LOTUS_ENABLE_OPAQUE_POINTERS 0
#endif

// API Changes
#if LLVM_VERSION_MAJOR >= 11
  #define LOTUS_HAS_ALIGN_IN_FUNCTION_ARGS 1
#else
  #define LOTUS_HAS_ALIGN_IN_FUNCTION_ARGS 0
#endif
