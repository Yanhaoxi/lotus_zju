#include "Alias/Spec/AliasSpecManager.h"
#include <llvm/Demangle/Demangle.h>

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <unordered_set>

using namespace lotus;
using namespace lotus::alias;

namespace {
// Build a list of candidate config directories, ordered by preference.
std::vector<std::string> candidateConfigDirs() {
  std::vector<std::string> dirs;

  if (const char *envPath = std::getenv("LOTUS_CONFIG_DIR")) {
    if (*envPath)
      dirs.emplace_back(envPath);
  }

  llvm::SmallString<256> cwd;
  if (!llvm::sys::fs::current_path(cwd)) {
    llvm::SmallString<256> configInCwd = cwd;
    llvm::sys::path::append(configInCwd, "config");
    dirs.emplace_back(configInCwd.str().str());

    llvm::SmallString<256> parent = cwd;
    llvm::sys::path::remove_filename(parent);
    llvm::SmallString<256> configInParent = parent;
    llvm::sys::path::append(configInParent, "config");
    dirs.emplace_back(configInParent.str().str());
  }

  // Deduplicate while preserving order
  std::vector<std::string> unique;
  std::unordered_set<std::string> seen;
  for (const auto &d : dirs) {
    if (seen.insert(d).second)
      unique.push_back(d);
  }
  return unique;
}

std::vector<std::string> findExistingSpecFiles(
    const std::vector<std::string> &specNames) {
  std::vector<std::string> results;
  std::unordered_set<std::string> seen;

  for (const auto &dir : candidateConfigDirs()) {
    for (const auto &name : specNames) {
      llvm::SmallString<256> path(dir);
      llvm::sys::path::append(path, name);
      std::string fullPath = path.str().str();
      if (seen.count(fullPath))
        continue;
      if (llvm::sys::fs::exists(fullPath)) {
        results.push_back(fullPath);
        seen.insert(fullPath);
      }
    }
  }
  return results;
}
}

// ===== Utility Functions =====

std::vector<std::string> lotus::alias::getDefaultSpecFiles() {
  static const std::vector<std::string> specNames = {"ptr.spec", "modref.spec"};
  auto existing = findExistingSpecFiles(specNames);
  if (!existing.empty())
    return existing;

  // Fallback to first candidate directory even if files don't yet exist,
  // preserving previous behavior.
  auto dirs = candidateConfigDirs();
  if (!dirs.empty()) {
    std::vector<std::string> candidates;
    for (const auto &name : specNames)
      candidates.push_back(dirs.front() + "/" + name);
    return candidates;
  }

  // Last resort: assume relative config/ like old behavior.
  return {"config/ptr.spec", "config/modref.spec"};
}

std::string lotus::alias::getSpecFilePath(const std::string &specFileName) {
  // Prefer the first candidate directory that exists.
  for (const auto &dir : candidateConfigDirs()) {
    llvm::SmallString<256> path(dir);
    llvm::sys::path::append(path, specFileName);
    if (llvm::sys::fs::exists(path))
      return path.str().str();
  }

  // Preserve original fallback behavior.
  return "config/" + specFileName;
}

const char* lotus::alias::categoryToString(FunctionCategory cat) {
  switch (cat) {
    case FunctionCategory::Unknown: return "Unknown";
    case FunctionCategory::Allocator: return "Allocator";
    case FunctionCategory::Deallocator: return "Deallocator";
    case FunctionCategory::Reallocator: return "Reallocator";
    case FunctionCategory::MemoryCopy: return "MemoryCopy";
    case FunctionCategory::MemorySet: return "MemorySet";
    case FunctionCategory::MemoryCompare: return "MemoryCompare";
    case FunctionCategory::StringOperation: return "StringOperation";
    case FunctionCategory::NoEffect: return "NoEffect";
    case FunctionCategory::ExitFunction: return "ExitFunction";
    case FunctionCategory::ReturnArgument: return "ReturnArgument";
    case FunctionCategory::IoOperation: return "IoOperation";
    case FunctionCategory::MathFunction: return "MathFunction";
    default: return "Unknown";
  }
}

llvm::Optional<FunctionCategory> lotus::alias::stringToCategory(const std::string &str) {
  if (str == "Allocator") return FunctionCategory::Allocator;
  if (str == "Deallocator") return FunctionCategory::Deallocator;
  if (str == "Reallocator") return FunctionCategory::Reallocator;
  if (str == "MemoryCopy") return FunctionCategory::MemoryCopy;
  if (str == "MemorySet") return FunctionCategory::MemorySet;
  if (str == "NoEffect") return FunctionCategory::NoEffect;
  if (str == "ExitFunction") return FunctionCategory::ExitFunction;
  if (str == "ReturnArgument") return FunctionCategory::ReturnArgument;
  return llvm::None;
}
