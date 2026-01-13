#pragma once

#include <llvm/Config/llvm-config.h>

// Centralized version helpers for LLVM 12.x -> latest support.
//
// Notes:
// - Prefer using these macros instead of sprinkling raw LLVM_VERSION_MAJOR checks
//   throughout the codebase.
// - Keep them header-only and dependency-free.

#define LOTUS_LLVM_VERSION_ENCODE(major, minor) (((major) * 100) + (minor))
#define LOTUS_LLVM_VERSION \
  LOTUS_LLVM_VERSION_ENCODE(LLVM_VERSION_MAJOR, LLVM_VERSION_MINOR)

#define LOTUS_LLVM_VERSION_AT_LEAST(major, minor) \
  (LOTUS_LLVM_VERSION >= LOTUS_LLVM_VERSION_ENCODE((major), (minor)))

#define LOTUS_LLVM_VERSION_OLDER_THAN(major, minor) \
  (LOTUS_LLVM_VERSION < LOTUS_LLVM_VERSION_ENCODE((major), (minor)))

// Feature toggles (best-effort; should be expanded as needs arise)
#if LLVM_VERSION_MAJOR >= 15
#define LOTUS_LLVM_OPAQUE_POINTERS_DEFAULT 1
#else
#define LOTUS_LLVM_OPAQUE_POINTERS_DEFAULT 0
#endif

