/**
 * \file WriteIR.h
 * \brief LLVM IR writing utilities
 * \author Lotus Team
 *
 * This file provides utilities for writing LLVM IR to files.
 * Supports both bitcode (.bc) and textual IR (.ll) formats.
 */
#pragma once

namespace llvm {
class Module;
} // namespace llvm

namespace util {
namespace io {

/**
 * \brief Write a module to a text file in LLVM IR format (.ll)
 * \param module The module to write
 * \param fileName Path to the output file
 */
void writeModuleToText(const llvm::Module &module, const char *fileName);

/**
 * \brief Write a module to a bitcode file (.bc)
 * \param module The module to write
 * \param fileName Path to the output file
 */
void writeModuleToBitCode(const llvm::Module &module, const char *fileName);

/**
 * \brief Write a module to a file with automatic format detection
 * \param module The module to write
 * \param fileName Path to the output file
 * \param isText If true, write text format (.ll); otherwise write bitcode (.bc)
 */
void writeModuleToFile(const llvm::Module &module, const char *fileName,
                       bool isText);

} // namespace io
} // namespace util