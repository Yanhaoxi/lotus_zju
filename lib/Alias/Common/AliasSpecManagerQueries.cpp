#include "Alias/Common/AliasSpecManager.h"
#include <llvm/IR/Intrinsics.h>
#include <llvm/Support/raw_ostream.h>
#include <algorithm>

using namespace lotus;
using namespace lotus::alias;

// ===== Category Queries =====

FunctionCategory AliasSpecManager::categorizeIntrinsic(const llvm::Function *F) const {
  if (!F || !F->isIntrinsic())
    return FunctionCategory::Unknown;

  using namespace llvm;
  switch (F->getIntrinsicID()) {
    case Intrinsic::memcpy:
    case Intrinsic::memmove:
      return FunctionCategory::MemoryCopy;
    case Intrinsic::memset:
      return FunctionCategory::MemorySet;
    case Intrinsic::expect:
    case Intrinsic::assume:
      return FunctionCategory::NoEffect;
    default:
      return FunctionCategory::Unknown;
  }
}

FunctionCategory AliasSpecManager::categorizeFunctionSpec(const FunctionSpec &spec) const {
  // Priority order for categorization
  if (spec.isExit) return FunctionCategory::ExitFunction;
  if (spec.isDeallocator) return FunctionCategory::Deallocator;
  if (spec.isAllocator) {
    // Check if it's also a copy (like realloc)
    if (!spec.copies.empty())
      return FunctionCategory::Reallocator;
    return FunctionCategory::Allocator;
  }
  if (spec.isIgnored) return FunctionCategory::NoEffect;
  
  if (!spec.copies.empty()) {
    // Determine if it's a memory copy or return argument
    for (const auto &copy : spec.copies) {
      if (copy.dstQualifier == QualifierKind::Region && copy.srcQualifier == QualifierKind::Region)
        return FunctionCategory::MemoryCopy;
      if (copy.dst.kind == SelectorKind::Ret)
        return FunctionCategory::ReturnArgument;
    }
    return FunctionCategory::StringOperation;
  }
  
  if (!spec.modref.empty())
    return FunctionCategory::IoOperation;
  
  return FunctionCategory::Unknown;
}

std::set<FunctionCategory> AliasSpecManager::categorizeFunctionSpecMulti(
    const FunctionSpec &spec) const {
  std::set<FunctionCategory> cats;
  
  if (spec.isExit) cats.insert(FunctionCategory::ExitFunction);
  if (spec.isDeallocator) cats.insert(FunctionCategory::Deallocator);
  if (spec.isAllocator) {
    if (!spec.copies.empty())
      cats.insert(FunctionCategory::Reallocator);
    else
      cats.insert(FunctionCategory::Allocator);
  }
  if (spec.isIgnored) cats.insert(FunctionCategory::NoEffect);
  
  for (const auto &copy : spec.copies) {
    if (copy.dstQualifier == QualifierKind::Region && copy.srcQualifier == QualifierKind::Region)
      cats.insert(FunctionCategory::MemoryCopy);
    if (copy.dst.kind == SelectorKind::Ret)
      cats.insert(FunctionCategory::ReturnArgument);
  }
  
  if (!spec.modref.empty())
    cats.insert(FunctionCategory::IoOperation);
  
  return cats;
}

FunctionCategory AliasSpecManager::getCategory(const llvm::Function *F) const {
  if (!F) return FunctionCategory::Unknown;

  const std::string name = normalizeFunctionName(F);
  FunctionCategory cat = getCategory(name);
  if (cat == FunctionCategory::Unknown) {
    cat = categorizeIntrinsic(F);
    if (cacheEnabled_)
      categoryCache_[name] = cat;
  }
  return cat;
}

FunctionCategory AliasSpecManager::getCategory(const std::string &functionName) const {
  if (cacheEnabled_) {
    auto it = categoryCache_.find(functionName);
    if (it != categoryCache_.end())
      return it->second;
  }
  
  const FunctionSpec *spec = lookupSpec(functionName);
  FunctionCategory cat = spec ? categorizeFunctionSpec(*spec) 
                              : FunctionCategory::Unknown;
  
  if (cacheEnabled_)
    categoryCache_[functionName] = cat;
  
  return cat;
}

std::set<FunctionCategory> AliasSpecManager::getCategories(const llvm::Function *F) const {
  if (!F) return {};

  const std::string name = normalizeFunctionName(F);
  auto cats = getCategories(name);

  if (cats.empty()) {
    FunctionCategory intrinsic = categorizeIntrinsic(F);
    if (intrinsic != FunctionCategory::Unknown)
      cats.insert(intrinsic);
    if (cacheEnabled_)
      categoriesCache_[name] = cats;
  }
  return cats;
}

std::set<FunctionCategory> AliasSpecManager::getCategories(
    const std::string &functionName) const {
  if (cacheEnabled_) {
    auto it = categoriesCache_.find(functionName);
    if (it != categoriesCache_.end())
      return it->second;
  }
  
  const FunctionSpec *spec = lookupSpec(functionName);
  std::set<FunctionCategory> cats = spec ? categorizeFunctionSpecMulti(*spec) 
                                         : std::set<FunctionCategory>{};
  
  if (cacheEnabled_)
    categoriesCache_[functionName] = cats;
  
  return cats;
}

// ===== Allocator Queries =====

bool AliasSpecManager::isAllocator(const llvm::Function *F) const {
  return F && isAllocator(normalizeFunctionName(F));
}

bool AliasSpecManager::isAllocator(const std::string &functionName) const {
  const FunctionSpec *spec = lookupSpec(functionName);
  return spec && spec->isAllocator;
}

AllocatorInfo AliasSpecManager::buildAllocatorInfo(
    const std::string &name, const FunctionSpec &spec) const {
  AllocatorInfo info;
  info.functionName = name;
  info.returnsPointer = true;
  info.ptrOutArgIndex = -1;
  info.initializesToZero = false;
  
  if (!spec.allocs.empty()) {
    info.sizeArgIndex = spec.allocs[0].sizeArgIndex;
  } else {
    info.sizeArgIndex = -1;
  }
  
  // Special cases
  if (name == "calloc") {
    info.initializesToZero = true;
    info.sizeArgIndex = 1; // calloc(count, size)
  } else if (name == "posix_memalign") {
    info.returnsPointer = false;
    info.ptrOutArgIndex = 0;
    info.sizeArgIndex = 2;
  }
  
  return info;
}

llvm::Optional<AllocatorInfo> AliasSpecManager::getAllocatorInfo(
    const llvm::Function *F) const {
  if (!F) return llvm::None;
  return getAllocatorInfo(normalizeFunctionName(F));
}

llvm::Optional<AllocatorInfo> AliasSpecManager::getAllocatorInfo(
    const std::string &functionName) const {
  if (cacheEnabled_) {
    auto it = allocatorCache_.find(functionName);
    if (it != allocatorCache_.end())
      return it->second;
  }
  
  const FunctionSpec *spec = lookupSpec(functionName);
  llvm::Optional<AllocatorInfo> result = llvm::None;
  
  if (spec && spec->isAllocator) {
    result = buildAllocatorInfo(functionName, *spec);
  }
  
  if (cacheEnabled_)
    allocatorCache_[functionName] = result;
  
  return result;
}

// ===== Deallocator Queries =====

bool AliasSpecManager::isDeallocator(const llvm::Function *F) const {
  return F && isDeallocator(normalizeFunctionName(F));
}

bool AliasSpecManager::isDeallocator(const std::string &functionName) const {
  return isKnownDeallocator(functionName);
}

// ===== No-Effect Queries =====

bool AliasSpecManager::isNoEffect(const llvm::Function *F) const {
  if (!F) return false;
  if (isNoEffect(normalizeFunctionName(F)))
    return true;
  return categorizeIntrinsic(F) == FunctionCategory::NoEffect;
}

bool AliasSpecManager::isNoEffect(const std::string &functionName) const {
  const FunctionSpec *spec = lookupSpec(functionName);
  return spec && spec->isIgnored;
}

// ===== Copy Operation Queries =====

bool AliasSpecManager::isMemoryCopy(const llvm::Function *F) const {
  if (!F) return false;
  if (isMemoryCopy(normalizeFunctionName(F)))
    return true;
  return categorizeIntrinsic(F) == FunctionCategory::MemoryCopy;
}

bool AliasSpecManager::isMemoryCopy(const std::string &functionName) const {
  const FunctionSpec *spec = lookupSpec(functionName);
  if (!spec)
    return false;

  for (const auto &copy : spec->copies) {
    if (copy.dstQualifier == QualifierKind::Region &&
        copy.srcQualifier == QualifierKind::Region)
      return true;
  }
  return false;
}

std::vector<CopyInfo> AliasSpecManager::buildCopyInfo(const FunctionSpec &spec) const {
  std::vector<CopyInfo> result;
  
  for (const auto &copy : spec.copies) {
    CopyInfo info;
    
    // Destination
    if (copy.dst.kind == SelectorKind::Arg) {
      info.dstArgIndex = copy.dst.index;
      info.dstIsRegion = (copy.dstQualifier == QualifierKind::Region);
    } else if (copy.dst.kind == SelectorKind::Ret) {
      info.dstArgIndex = -1; // Return value
      info.dstIsRegion = (copy.dstQualifier == QualifierKind::Region);
    }
    
    // Source
    if (copy.src.kind == SelectorKind::Arg) {
      info.srcArgIndex = copy.src.index;
      info.srcIsRegion = (copy.srcQualifier == QualifierKind::Region);
    } else if (copy.src.kind == SelectorKind::Static) {
      info.srcArgIndex = -1;
      info.srcIsRegion = false;
    } else if (copy.src.kind == SelectorKind::Null) {
      info.srcArgIndex = -1;
      info.srcIsRegion = false;
    }
    
    // Return aliasing
    if (copy.dst.kind == SelectorKind::Ret) {
      info.returnsAlias = true;
      info.retArgIndex = copy.src.kind == SelectorKind::Arg ? copy.src.index : -1;
    } else {
      info.returnsAlias = false;
      info.retArgIndex = -1;
    }
    
    result.push_back(info);
  }
  
  return result;
}

std::vector<CopyInfo> AliasSpecManager::buildIntrinsicCopyInfo(
    const llvm::Function *F) const {
  std::vector<CopyInfo> result;
  if (!F || !F->isIntrinsic())
    return result;

  switch (F->getIntrinsicID()) {
    case llvm::Intrinsic::memcpy:
    case llvm::Intrinsic::memmove: {
      CopyInfo info;
      info.dstArgIndex = 0;
      info.srcArgIndex = 1;
      info.dstIsRegion = true;
      info.srcIsRegion = true;
      info.returnsAlias = true;
      info.retArgIndex = 0;
      result.push_back(info);
      break;
    }
    default:
      break;
  }
  return result;
}

std::vector<CopyInfo> AliasSpecManager::getCopyEffects(const llvm::Function *F) const {
  if (!F) return {};
  auto copies = getCopyEffects(normalizeFunctionName(F));
  if (!copies.empty())
    return copies;

  // Fallback for well-known intrinsics
  auto intrinsicCopies = buildIntrinsicCopyInfo(F);
  if (cacheEnabled_)
    copyCache_[normalizeFunctionName(F)] = intrinsicCopies;
  return intrinsicCopies;
}

std::vector<CopyInfo> AliasSpecManager::getCopyEffects(
    const std::string &functionName) const {
  if (cacheEnabled_) {
    auto it = copyCache_.find(functionName);
    if (it != copyCache_.end())
      return it->second;
  }
  
  const FunctionSpec *spec = lookupSpec(functionName);
  std::vector<CopyInfo> result = spec ? buildCopyInfo(*spec) : std::vector<CopyInfo>{};
  
  if (cacheEnabled_)
    copyCache_[functionName] = result;
  
  return result;
}

// ===== Return Alias Queries =====

bool AliasSpecManager::returnsArgumentAlias(const llvm::Function *F) const {
  if (!F) return false;
  if (returnsArgumentAlias(normalizeFunctionName(F)))
    return true;
  // memcpy/memmove return the destination pointer
  return categorizeIntrinsic(F) == FunctionCategory::MemoryCopy;
}

bool AliasSpecManager::returnsArgumentAlias(const std::string &functionName) const {
  const FunctionSpec *spec = lookupSpec(functionName);
  if (!spec)
    return false;

  for (const auto &copy : spec->copies) {
    if (copy.dst.kind == SelectorKind::Ret)
      return true;
  }
  return false;
}

std::vector<ReturnAliasInfo> AliasSpecManager::buildReturnAliasInfo(
    const FunctionSpec &spec) const {
  std::vector<ReturnAliasInfo> result;
  
  for (const auto &copy : spec.copies) {
    if (copy.dst.kind != SelectorKind::Ret)
      continue;
    
    ReturnAliasInfo info;
    info.argIndex = copy.src.kind == SelectorKind::Arg ? copy.src.index : -1;
    info.isRegion = (copy.dstQualifier == QualifierKind::Region);
    info.isStatic = (copy.src.kind == SelectorKind::Static);
    info.isNull = (copy.src.kind == SelectorKind::Null);
    
    result.push_back(info);
  }
  
  return result;
}

std::vector<ReturnAliasInfo> AliasSpecManager::getReturnAliasInfo(
    const llvm::Function *F) const {
  if (!F) return {};
  std::string name = normalizeFunctionName(F);
  auto info = getReturnAliasInfo(name);
  if (!info.empty())
    return info;

  if (categorizeIntrinsic(F) == FunctionCategory::MemoryCopy) {
    ReturnAliasInfo intrinsicInfo;
    intrinsicInfo.argIndex = 0;
    intrinsicInfo.isRegion = false;
    intrinsicInfo.isStatic = false;
    intrinsicInfo.isNull = false;
    std::vector<ReturnAliasInfo> intrinsic = {intrinsicInfo};
    if (cacheEnabled_)
      returnAliasCache_[name] = intrinsic;
    return intrinsic;
  }
  return {};
}

std::vector<ReturnAliasInfo> AliasSpecManager::getReturnAliasInfo(
    const std::string &functionName) const {
  if (cacheEnabled_) {
    auto it = returnAliasCache_.find(functionName);
    if (it != returnAliasCache_.end())
      return it->second;
  }
  
  const FunctionSpec *spec = lookupSpec(functionName);
  std::vector<ReturnAliasInfo> result = spec ? buildReturnAliasInfo(*spec) 
                                             : std::vector<ReturnAliasInfo>{};
  
  if (cacheEnabled_)
    returnAliasCache_[functionName] = result;
  
  return result;
}

// ===== Exit Function Queries =====

bool AliasSpecManager::isExitFunction(const llvm::Function *F) const {
  return F && isExitFunction(normalizeFunctionName(F));
}

bool AliasSpecManager::isExitFunction(const std::string &functionName) const {
  const FunctionSpec *spec = lookupSpec(functionName);
  return spec && spec->isExit;
}

// ===== Mod/Ref Queries =====

ModRefInfo AliasSpecManager::buildModRefInfo(const FunctionSpec &spec) const {
  ModRefInfo info;
  
  for (const auto &mr : spec.modref) {
    if (mr.target.kind == SelectorKind::Arg) {
      int argIdx = mr.target.index;
      if (mr.op == SpecOpKind::Mod) {
        info.modifiedArgs.push_back(argIdx);
      } else if (mr.op == SpecOpKind::Ref) {
        info.referencedArgs.push_back(argIdx);
      }
    } else if (mr.target.kind == SelectorKind::Ret) {
      if (mr.op == SpecOpKind::Mod) {
        info.modifiesReturn = true;
      } else if (mr.op == SpecOpKind::Ref) {
        info.referencesReturn = true;
      }
    }
  }
  
  return info;
}

ModRefInfo AliasSpecManager::buildIntrinsicModRefInfo(const llvm::Function *F) const {
  ModRefInfo info;
  if (!F || !F->isIntrinsic())
    return info;

  switch (F->getIntrinsicID()) {
    case llvm::Intrinsic::memcpy:
    case llvm::Intrinsic::memmove:
      info.modifiedArgs.push_back(0);
      info.referencedArgs.push_back(1);
      break;
    case llvm::Intrinsic::memset:
      info.modifiedArgs.push_back(0);
      break;
    default:
      break;
  }
  return info;
}

ModRefInfo AliasSpecManager::getModRefInfo(const llvm::Function *F) const {
  if (!F) return ModRefInfo();

  const std::string name = normalizeFunctionName(F);
  if (cacheEnabled_) {
    auto it = modRefCache_.find(name);
    if (it != modRefCache_.end())
      return it->second;
  }

  const FunctionSpec *spec = lookupSpec(name);
  ModRefInfo result = spec ? buildModRefInfo(*spec) : buildIntrinsicModRefInfo(F);

  if (cacheEnabled_)
    modRefCache_[name] = result;

  return result;
}

ModRefInfo AliasSpecManager::getModRefInfo(const std::string &functionName) const {
  if (cacheEnabled_) {
    auto it = modRefCache_.find(functionName);
    if (it != modRefCache_.end())
      return it->second;
  }
  
  const FunctionSpec *spec = lookupSpec(functionName);
  ModRefInfo result = spec ? buildModRefInfo(*spec) : ModRefInfo();
  
  if (cacheEnabled_)
    modRefCache_[functionName] = result;
  
  return result;
}

bool AliasSpecManager::modifiesArg(const llvm::Function *F, int argIndex) const {
  auto info = getModRefInfo(F);
  return std::find(info.modifiedArgs.begin(), info.modifiedArgs.end(), argIndex) 
         != info.modifiedArgs.end();
}

bool AliasSpecManager::referencesArg(const llvm::Function *F, int argIndex) const {
  auto info = getModRefInfo(F);
  return std::find(info.referencedArgs.begin(), info.referencedArgs.end(), argIndex) 
         != info.referencedArgs.end();
}

// ===== Batch Queries =====

void AliasSpecManager::buildCategoryLists() const {
  if (categoryListsBuilt_) return;
  
  categoryLists_.clear();
  
  for (const auto &entry : apiSpec_.all()) {
    const std::string &name = entry.first;
    const FunctionSpec &spec = entry.second;
    
    auto cats = categorizeFunctionSpecMulti(spec);
    for (auto cat : cats) {
      categoryLists_[cat].push_back(name);
    }
  }
  
  categoryListsBuilt_ = true;
}

std::vector<std::string> AliasSpecManager::getFunctionsByCategory(
    FunctionCategory cat) const {
  buildCategoryLists();
  
  auto it = categoryLists_.find(cat);
  if (it != categoryLists_.end())
    return it->second;
  return {};
}

std::vector<std::string> AliasSpecManager::getAllocatorNames() const {
  return getFunctionsByCategory(FunctionCategory::Allocator);
}

std::vector<std::string> AliasSpecManager::getDeallocatorNames() const {
  return getFunctionsByCategory(FunctionCategory::Deallocator);
}

std::vector<std::string> AliasSpecManager::getNoEffectNames() const {
  return getFunctionsByCategory(FunctionCategory::NoEffect);
}
