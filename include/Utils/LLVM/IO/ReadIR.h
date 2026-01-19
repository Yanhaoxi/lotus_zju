/**
 * \file ReadIR.h
 * \brief LLVM IR reading utilities
 * \author Lotus Team
 *
 * This file provides utilities for reading LLVM IR from files and strings.
 * Supports both bitcode (.bc) and textual IR (.ll) formats.
 */
#pragma once

#include <memory>

namespace llvm {
class Module;
} // namespace llvm

namespace util {
namespace io {

/**
 * \brief Read an LLVM module from a file
 * \param fileName Path to the file (supports .bc bitcode and .ll text format)
 * \return Unique pointer to the loaded module, or nullptr on failure
 */
std::unique_ptr<llvm::Module> readModuleFromFile(const char *fileName);

/**
 * \brief Read an LLVM module from a string containing IR
 * \param assembly String containing LLVM IR in text format
 * \return Unique pointer to the parsed module, or nullptr on failure
 */
std::unique_ptr<llvm::Module> readModuleFromString(const char *assembly);

} // namespace io
} // namespace util
