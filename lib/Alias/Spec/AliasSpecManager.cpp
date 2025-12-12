#include "Alias/Spec/AliasSpecManager.h"
#include <llvm/Demangle/Demangle.h>
#include <llvm/Support/raw_ostream.h>
#include <cctype>

using namespace lotus;
using namespace lotus::alias;

// ===== AliasSpecManager Implementation =====

AliasSpecManager::AliasSpecManager() 
  : module_(nullptr), cacheEnabled_(true), categoryListsBuilt_(false) {
  std::string err;
  for (const auto &path : getDefaultSpecFiles()) {
    if (apiSpec_.loadFile(path, err)) {
      loadedSpecFiles_.push_back(path);
    } else if (!err.empty()) {
      llvm::errs() << "[AliasSpecManager] Warning: " << err << "\n";
    }
  }
}

AliasSpecManager::AliasSpecManager(const std::vector<std::string> &specFilePaths)
  : module_(nullptr), cacheEnabled_(true), categoryListsBuilt_(false) {
  std::string err;
  for (const auto &path : specFilePaths) {
    if (apiSpec_.loadFile(path, err)) {
      loadedSpecFiles_.push_back(path);
    } else if (!err.empty()) {
      llvm::errs() << "[AliasSpecManager] Warning: " << err << "\n";
    }
  }
}

void AliasSpecManager::initialize(const llvm::Module &M) {
  module_ = &M;
  clearCache();
}

bool AliasSpecManager::loadSpecFile(const std::string &path, std::string &errorMsg) {
  bool result = apiSpec_.loadFile(path, errorMsg);
  if (result) {
    loadedSpecFiles_.push_back(path);
    clearCache();
  } else if (!errorMsg.empty()) {
    llvm::errs() << "[AliasSpecManager] Failed to load spec file " << path
                 << ": " << errorMsg << "\n";
  }
  return result;
}

std::string AliasSpecManager::normalizeFunctionName(const llvm::Function *F) const {
  if (!F) return "";
  return F->getName().str();
}

std::string AliasSpecManager::demangle(const std::string &mangledName) const {
  if (mangledName.empty() || mangledName[0] != '_')
    return mangledName;
  
  std::string demangled = llvm::demangle(mangledName);
  return demangled.empty() ? mangledName : demangled;
}

std::string AliasSpecManager::canonicalizeName(const std::string &name) const {
  if (name.empty())
    return name;

  std::string result = name;
  if (!result.empty() && result[0] == '_')
    result = result.substr(1);

  // Strip arguments from demangled names (e.g., "foo(int)" -> "foo")
  auto parenPos = result.find('(');
  if (parenPos != std::string::npos)
    result = result.substr(0, parenPos);

  // Remove trailing spaces that may appear in demangled names
  while (!result.empty() && std::isspace(static_cast<unsigned char>(result.back())))
    result.pop_back();

  return result;
}

const FunctionSpec *AliasSpecManager::lookupSpec(
    const std::string &functionName) const {
  if (functionName.empty())
    return nullptr;

  if (const FunctionSpec *spec = apiSpec_.get(functionName))
    return spec;

  // Try canonicalized version (strip leading underscore and signature)
  std::string canonical = canonicalizeName(functionName);
  if (canonical != functionName) {
    if (const FunctionSpec *spec = apiSpec_.get(canonical))
      return spec;
  }

  // Try demangled name (for C++ mangled symbols)
  std::string demangled = demangle(functionName);
  if (demangled != functionName) {
    demangled = canonicalizeName(demangled);
    if (const FunctionSpec *spec = apiSpec_.get(demangled))
      return spec;
  }

  return nullptr;
}

bool AliasSpecManager::isKnownDeallocator(const std::string &functionName) const {
  const FunctionSpec *spec = lookupSpec(functionName);
  return spec && spec->isDeallocator;
}

// ===== Configuration =====

void AliasSpecManager::setCacheEnabled(bool enabled) {
  cacheEnabled_ = enabled;
  if (!enabled) clearCache();
}

void AliasSpecManager::clearCache() {
  categoryCache_.clear();
  categoriesCache_.clear();
  allocatorCache_.clear();
  copyCache_.clear();
  returnAliasCache_.clear();
  modRefCache_.clear();
  categoryListsBuilt_ = false;
  categoryLists_.clear();
}

void AliasSpecManager::addCustomSpec(const std::string &functionName, 
                                     const FunctionSpec &spec) {
  FunctionSpec copy = spec;
  if (copy.functionName.empty())
    copy.functionName = functionName;
  apiSpec_.addOrReplaceSpec(copy);
  clearCache();
}

// ===== Debugging =====

void AliasSpecManager::printAllSpecs(llvm::raw_ostream &OS) const {
  for (const auto &entry : apiSpec_.all()) {
    OS << entry.first << ": ";
    const FunctionSpec &spec = entry.second;
    
    if (spec.isIgnored) OS << "IGNORE ";
    if (spec.isExit) OS << "EXIT ";
    if (spec.isAllocator) OS << "ALLOC ";
    if (!spec.copies.empty()) OS << "COPY(" << spec.copies.size() << ") ";
    if (!spec.modref.empty()) OS << "MODREF(" << spec.modref.size() << ") ";
    
    OS << "\n";
  }
}

AliasSpecManager::Statistics AliasSpecManager::getStatistics() const {
  Statistics stats;
  stats.totalFunctions = apiSpec_.all().size();
  stats.allocators = 0;
  stats.deallocators = 0;
  stats.noEffectFunctions = 0;
  stats.copyFunctions = 0;
  stats.exitFunctions = 0;
  
  for (const auto &entry : apiSpec_.all()) {
    const FunctionSpec &spec = entry.second;
    if (spec.isAllocator) stats.allocators++;
    if (spec.isIgnored) stats.noEffectFunctions++;
    if (!spec.copies.empty()) stats.copyFunctions++;
    if (spec.isExit) stats.exitFunctions++;
  }
  
  stats.deallocators = getDeallocatorNames().size();
  
  return stats;
}
