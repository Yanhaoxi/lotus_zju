// Unified Spec Management for Alias Analyses
// Provides high-level query interface for function specifications
// used by various alias analyses (SparrowAA, AllocAA, LotusAA, etc.)

#ifndef LOTUS_ALIAS_SPEC_ALIAS_SPEC_MANAGER_H
#define LOTUS_ALIAS_SPEC_ALIAS_SPEC_MANAGER_H

#include "Annotation/APISpec.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>

#include <memory>
#include <llvm/ADT/Optional.h>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace lotus {
namespace alias {

// Function categories relevant to alias analysis
enum class FunctionCategory {
  Unknown,
  Allocator,        // malloc, calloc, new
  Deallocator,      // free, delete
  Reallocator,      // realloc (allocates and copies)
  MemoryCopy,       // memcpy, memmove, bcopy
  MemorySet,        // memset
  MemoryCompare,    // memcmp
  StringOperation,  // strcpy, strcat, strlen, etc.
  NoEffect,         // Pure functions with no pointer effects
  ExitFunction,     // exit, abort, etc.
  ReturnArgument,   // Returns alias of specific argument (strcpy, fgets)
  IoOperation,      // File I/O, network I/O
  MathFunction,     // sqrt, sin, cos, etc.
};

// Information about allocator functions
struct AllocatorInfo {
  std::string functionName;
  int sizeArgIndex;        // Index of size argument (-1 if none/unknown)
  bool returnsPointer;     // true for malloc, false for posix_memalign
  int ptrOutArgIndex;      // For posix_memalign-style (-1 if not applicable)
  bool initializesToZero;  // true for calloc
  
  AllocatorInfo() 
    : sizeArgIndex(-1), returnsPointer(true), 
      ptrOutArgIndex(-1), initializesToZero(false) {}
};

// Information about memory copy operations
struct CopyInfo {
  int dstArgIndex;         // Destination argument index
  int srcArgIndex;         // Source argument index
  bool dstIsRegion;        // true: copy to *dst, false: copy to dst
  bool srcIsRegion;        // true: copy from *src, false: copy from src
  bool returnsAlias;       // true if return value aliases dst
  int retArgIndex;         // Which arg return aliases (-1 if Ret aliases dst)
  
  CopyInfo() 
    : dstArgIndex(-1), srcArgIndex(-1), 
      dstIsRegion(false), srcIsRegion(false),
      returnsAlias(false), retArgIndex(-1) {}
};

// Information about return value aliasing
struct ReturnAliasInfo {
  int argIndex;            // Which argument return value aliases (-1 for STATIC)
  bool isRegion;           // true: ret aliases *arg, false: ret aliases arg
  bool isStatic;           // true: returns static/global pointer
  bool isNull;             // true: returns null pointer
  
  ReturnAliasInfo() 
    : argIndex(-1), isRegion(false), 
      isStatic(false), isNull(false) {}
};

// Information about mod/ref behavior
struct ModRefInfo {
  std::vector<int> modifiedArgs;    // Arguments that are modified (written)
  std::vector<int> referencedArgs;  // Arguments that are read
  bool modifiesReturn;              // Return value region is modified
  bool referencesReturn;            // Return value region is read
  
  ModRefInfo() : modifiesReturn(false), referencesReturn(false) {}
};

// Main interface for querying function specifications
class AliasSpecManager {
public:
  // Default constructor: loads default spec files from config/
  AliasSpecManager();
  
  // Constructor: loads specified spec files
  explicit AliasSpecManager(const std::vector<std::string> &specFilePaths);
  
  // Initialize with LLVM module (optional, enables better name matching)
  void initialize(const llvm::Module &M);
  
  // Load additional spec file
  bool loadSpecFile(const std::string &path, std::string &errorMsg);
  
  // ===== Category Queries =====
  
  // Get primary category for a function
  FunctionCategory getCategory(const llvm::Function *F) const;
  FunctionCategory getCategory(const std::string &functionName) const;
  
  // Get all applicable categories (a function may have multiple)
  std::set<FunctionCategory> getCategories(const llvm::Function *F) const;
  std::set<FunctionCategory> getCategories(const std::string &functionName) const;
  
  // ===== Allocator Queries =====
  
  bool isAllocator(const llvm::Function *F) const;
  bool isAllocator(const std::string &functionName) const;
  
  llvm::Optional<AllocatorInfo> getAllocatorInfo(const llvm::Function *F) const;
  llvm::Optional<AllocatorInfo> getAllocatorInfo(const std::string &functionName) const;
  
  // ===== Deallocator Queries =====
  
  bool isDeallocator(const llvm::Function *F) const;
  bool isDeallocator(const std::string &functionName) const;
  
  // ===== No-Effect (Pure) Function Queries =====
  
  // Returns true if function has no pointer-related side effects
  bool isNoEffect(const llvm::Function *F) const;
  bool isNoEffect(const std::string &functionName) const;
  
  // ===== Copy/Memory Operation Queries =====
  
  bool isMemoryCopy(const llvm::Function *F) const;
  bool isMemoryCopy(const std::string &functionName) const;
  
  // Get all copy effects for a function
  std::vector<CopyInfo> getCopyEffects(const llvm::Function *F) const;
  std::vector<CopyInfo> getCopyEffects(const std::string &functionName) const;
  
  // ===== Return Alias Queries =====
  
  // Check if return value aliases an argument
  bool returnsArgumentAlias(const llvm::Function *F) const;
  bool returnsArgumentAlias(const std::string &functionName) const;
  
  // Get return alias information
  std::vector<ReturnAliasInfo> getReturnAliasInfo(const llvm::Function *F) const;
  std::vector<ReturnAliasInfo> getReturnAliasInfo(const std::string &functionName) const;
  
  // ===== Exit Function Queries =====
  
  bool isExitFunction(const llvm::Function *F) const;
  bool isExitFunction(const std::string &functionName) const;
  
  // ===== Mod/Ref Queries =====
  
  ModRefInfo getModRefInfo(const llvm::Function *F) const;
  ModRefInfo getModRefInfo(const std::string &functionName) const;
  
  // Convenience methods
  bool modifiesArg(const llvm::Function *F, int argIndex) const;
  bool referencesArg(const llvm::Function *F, int argIndex) const;
  
  // ===== Batch Queries (for Analysis Initialization) =====
  
  // Get all known functions of a specific category
  std::vector<std::string> getFunctionsByCategory(FunctionCategory cat) const;
  
  // Get all known allocator function names
  std::vector<std::string> getAllocatorNames() const;
  
  // Get all known deallocator function names  
  std::vector<std::string> getDeallocatorNames() const;
  
  // Get all no-effect function names
  std::vector<std::string> getNoEffectNames() const;
  
  // ===== Configuration =====
  
  // Enable/disable result caching (default: enabled)
  void setCacheEnabled(bool enabled);
  
  // Clear all caches (useful after loading new spec files)
  void clearCache();
  
  // Add custom specification programmatically
  void addCustomSpec(const std::string &functionName, const FunctionSpec &spec);
  
  // Get underlying APISpec for advanced queries
  const APISpec& getAPISpec() const { return apiSpec_; }
  const std::vector<std::string>& getLoadedSpecFiles() const { return loadedSpecFiles_; }
  
  // ===== Debugging/Statistics =====
  
  // Print all loaded specifications
  void printAllSpecs(llvm::raw_ostream &OS) const;
  
  // Get statistics about loaded specs
  struct Statistics {
    size_t totalFunctions;
    size_t allocators;
    size_t deallocators;
    size_t noEffectFunctions;
    size_t copyFunctions;
    size_t exitFunctions;
  };
  Statistics getStatistics() const;
  
private:
  APISpec apiSpec_;
  const llvm::Module *module_;
  bool cacheEnabled_;
  std::vector<std::string> loadedSpecFiles_;
  
  // Caches for performance
  mutable std::unordered_map<std::string, FunctionCategory> categoryCache_;
  mutable std::unordered_map<std::string, std::set<FunctionCategory>> categoriesCache_;
  mutable std::unordered_map<std::string, llvm::Optional<AllocatorInfo>> allocatorCache_;
  mutable std::unordered_map<std::string, std::vector<CopyInfo>> copyCache_;
  mutable std::unordered_map<std::string, std::vector<ReturnAliasInfo>> returnAliasCache_;
  mutable std::unordered_map<std::string, ModRefInfo> modRefCache_;
  
  // Pre-computed category lists (built during initialization)
  mutable std::unordered_map<FunctionCategory, std::vector<std::string>> categoryLists_;
  mutable bool categoryListsBuilt_;
  
  // Helper methods
  std::string normalizeFunctionName(const llvm::Function *F) const;
  std::string demangle(const std::string &mangledName) const;
  std::string canonicalizeName(const std::string &name) const;
  const FunctionSpec* lookupSpec(const std::string &functionName) const;
  FunctionCategory categorizeIntrinsic(const llvm::Function *F) const;
  bool isKnownDeallocator(const std::string &functionName) const;
  
  FunctionCategory categorizeFunctionSpec(const FunctionSpec &spec) const;
  std::set<FunctionCategory> categorizeFunctionSpecMulti(const FunctionSpec &spec) const;
  
  AllocatorInfo buildAllocatorInfo(const std::string &name, const FunctionSpec &spec) const;
  std::vector<CopyInfo> buildCopyInfo(const FunctionSpec &spec) const;
  std::vector<CopyInfo> buildIntrinsicCopyInfo(const llvm::Function *F) const;
  std::vector<ReturnAliasInfo> buildReturnAliasInfo(const FunctionSpec &spec) const;
  ModRefInfo buildModRefInfo(const FunctionSpec &spec) const;
  ModRefInfo buildIntrinsicModRefInfo(const llvm::Function *F) const;
  
  void buildCategoryLists() const;
};

// ===== Utility Functions =====

// Get default spec file paths (from LOTUS_CONFIG_DIR or relative to binary)
std::vector<std::string> getDefaultSpecFiles();

// Get path to a specific spec file
std::string getSpecFilePath(const std::string &specFileName);

// Convert category to string (for debugging)
const char* categoryToString(FunctionCategory cat);

// Parse category from string
llvm::Optional<FunctionCategory> stringToCategory(const std::string &str);

} // namespace alias
} // namespace lotus

#endif // LOTUS_ALIAS_COMMON_ALIAS_SPEC_MANAGER_H
