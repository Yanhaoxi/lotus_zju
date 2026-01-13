#include "Alias/TPA/Util/IO/WriteIR.h"
#include "Alias/TPA/Util/Log.h"

#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/ToolOutputFile.h>

using namespace llvm;

namespace util {
namespace io {

void writeModuleToText(const Module &module, const char *fileName) {
  std::error_code ec;
  ToolOutputFile out(fileName, ec, sys::fs::OF_None);
  if (ec) {
    LOG_ERROR("Failed to write module to text file {}: {}", fileName, ec.message());
    std::exit(-3);
  }

  module.print(out.os(), nullptr);

  out.keep();
}

void writeModuleToBitCode(const Module &module, const char *fileName) {
  std::error_code ec;
  raw_fd_ostream out(fileName, ec, sys::fs::OF_None);
  if (ec) {
    LOG_ERROR("Failed to write module to bitcode file {}: {}", fileName, ec.message());
    std::exit(-3);
  }

  WriteBitcodeToFile(module, out);
}

void writeModuleToFile(const Module &module, const char *fileName,
                       bool isText) {
  if (isText)
    writeModuleToText(module, fileName);
  else
    writeModuleToBitCode(module, fileName);
}

} // namespace io
} // namespace util
