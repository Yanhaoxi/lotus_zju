#include "Verification/SymbolicAbstraction/Utils/Utils.h"

#include "Verification/SymbolicAbstraction/Core/repr.h"

#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Path.h>
#include <llvm/Transforms/Utils/PromoteMemToReg.h>

namespace symbolic_abstraction {
namespace // unnamed
{
class VerboseOutBuf : public std::stringbuf {
protected:
  virtual int sync() {
    if (VerboseEnable)
      std::cerr << str();

    str("");
    return 0;
  }
} VerboseOutBufInstance;
} // namespace

bool VerboseEnable;
std::ostream vout(&VerboseOutBufInstance);

void panic(const std::string &in) {
  std::cerr << "Error: " << in << '\n';
  exit(13);
}

std::string escapeJSON(const std::string &in) {
  std::stringstream ss;

  for (unsigned c : in) {
    if (c < '\x20' || c == '\\' || c == '"')
      ss << "\\u" << std::setfill('0') << std::setw(4) << std::hex << c;
    else
      ss << (char)c;
  }

  return ss.str();
}

std::string escapeHTML(const std::string &in) {
  std::stringstream ss;

  for (char c : in) {
    switch (c) {
    case '<':
      ss << "&lt;";
      break;

    case '>':
      ss << "&gt;";
      break;

    case '&':
      ss << "&amp;";
      break;

    default:
      if (std::isprint(c))
        ss << (char)c;
    }
  }

  return ss.str();
}

std::string getFunctionSourcePath(const llvm::Function *function) {
  // Try to get debug info from the first instruction
  for (const auto &bb : *function) {
    for (const auto &inst : bb) {
      if (const llvm::DILocation *debugLoc = inst.getDebugLoc()) {
        if (debugLoc->getFile()) {
          llvm::SmallString<256> path = debugLoc->getDirectory();
          llvm::sys::path::append(path, debugLoc->getFilename());
          return path.str().str();
        }
      }
    }
  }
  return "";
}

unique_ptr<llvm::Module> loadModule(std::string file_name) {
  auto err_or_file = llvm::MemoryBuffer::getFile(file_name);

  if (!err_or_file)
    throw std::runtime_error("Cannot load file: `" + file_name + "'");

  llvm::LLVMContext context;
  auto err_or_module =
      llvm::parseBitcodeFile(err_or_file.get()->getMemBufferRef(), context);

  if (!err_or_module)
    throw std::runtime_error("Cannot parse bitcode file: `" + file_name + "'");

  return std::move(err_or_module.get());
}

bool isInSSAForm(llvm::Function *function) {
  using namespace llvm;

  for (BasicBlock &bb : *function) {
    for (Instruction &inst : bb) {
      if (auto *ai = dyn_cast<AllocaInst>(&inst)) {
        if (isAllocaPromotable(ai))
          return false;
      }
    }
  }

  return true;
}
} // end namespace symbolic_abstraction
