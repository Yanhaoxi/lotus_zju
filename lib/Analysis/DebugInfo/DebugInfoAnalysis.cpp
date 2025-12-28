// Use debug info of LLVM to better report bugs, e.g., line number, position, function name, variable name, etc.


#include "Analysis/DebugInfo/DebugInfoAnalysis.h"
#include "Utils/LLVM/Demangle.h"
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/IR/Argument.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Operator.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/raw_ostream.h>
#include <fstream>
#include <limits>
#include <unistd.h>

using namespace llvm;

// Static cache for source file contents
std::map<std::string, std::vector<std::string>> DebugInfoAnalysis::sourceFileCache;

DebugInfoAnalysis::DebugInfoAnalysis() = default;

//===----------------------------------------------------------------------===//
// Helper Functions (inspired by Phasar)
//===----------------------------------------------------------------------===//

// Get DbgVariableIntrinsic for a value (handles ValueAsMetadata and Arguments)
static DbgVariableIntrinsic *getDbgVarIntrinsic(const Value *V) {
  // Try ValueAsMetadata approach (more robust)
  if (auto *VAM = ValueAsMetadata::getIfExists(const_cast<Value *>(V))) {
    if (auto *MDV = MetadataAsValue::getIfExists(V->getContext(), VAM)) {
      for (auto *U : MDV->users()) {
        if (auto *DBGIntr = dyn_cast<DbgVariableIntrinsic>(U)) {
          return DBGIntr;
        }
      }
    }
  }
  
  // Handle Arguments: if mem2reg is not activated, formal parameters will be
  // stored in registers at the beginning of function call. Debug info will be
  // linked to those alloca's instead of the arguments itself.
  if (const auto *Arg = dyn_cast<Argument>(V)) {
    for (const auto *User : Arg->users()) {
      if (const auto *Store = dyn_cast<StoreInst>(User)) {
        if (Store->getValueOperand() == Arg &&
            isa<AllocaInst>(Store->getPointerOperand())) {
          return getDbgVarIntrinsic(Store->getPointerOperand());
        }
      }
    }
  }
  
  return nullptr;
}

// Get DILocalVariable from a Value
static DILocalVariable *getDILocalVariable(const Value *V) {
  if (auto *DbgIntr = getDbgVarIntrinsic(V)) {
    if (auto *DDI = dyn_cast<DbgDeclareInst>(DbgIntr)) {
      return DDI->getVariable();
    }
    if (auto *DVI = dyn_cast<DbgValueInst>(DbgIntr)) {
      return DVI->getVariable();
    }
  }
  return nullptr;
}

// Get DIGlobalVariable from a Value
static DIGlobalVariable *getDIGlobalVariable(const Value *V) {
  if (const auto *GV = dyn_cast<GlobalVariable>(V)) {
    if (auto *MN = GV->getMetadata(LLVMContext::MD_dbg)) {
      if (auto *DIGVExp = dyn_cast<DIGlobalVariableExpression>(MN)) {
        return DIGVExp->getVariable();
      }
    }
  }
  return nullptr;
}

// Get DILocation from a Value
static DILocation *getDILocation(const Value *V) {
  // Arguments and Instructions such as AllocaInst
  if (const auto *I = dyn_cast<Instruction>(V)) {
    if (auto *MN = I->getMetadata(LLVMContext::MD_dbg)) {
      return dyn_cast<DILocation>(MN);
    }
  }

  if (auto *DbgIntr = getDbgVarIntrinsic(V)) {
    if (auto *MN = DbgIntr->getMetadata(LLVMContext::MD_dbg)) {
      return dyn_cast<DILocation>(MN);
    }
  }

  return nullptr;
}

// Get DIFile from a Value
static const DIFile *getDIFileFromIR(const Value *V) {
  if (const auto *GO = dyn_cast<GlobalObject>(V)) {
    if (auto *MN = GO->getMetadata(LLVMContext::MD_dbg)) {
      if (auto *Subpr = dyn_cast<DISubprogram>(MN)) {
        return Subpr->getFile();
      }
      if (auto *GVExpr = dyn_cast<DIGlobalVariableExpression>(MN)) {
        return GVExpr->getVariable()->getFile();
      }
    }
  } else if (const auto *Arg = dyn_cast<Argument>(V)) {
    if (auto *LocVar = getDILocalVariable(Arg)) {
      return LocVar->getFile();
    }
  } else if (const auto *I = dyn_cast<Instruction>(V)) {
    if (I->isUsedByMetadata()) {
      if (auto *LocVar = getDILocalVariable(I)) {
        return LocVar->getFile();
      }
    } else if (I->getMetadata(LLVMContext::MD_dbg)) {
      return I->getDebugLoc()->getFile();
    }
    if (const auto *DIFun = I->getFunction()->getSubprogram()) {
      return DIFun->getFile();
    }
  }
  return nullptr;
}

//===----------------------------------------------------------------------===//
// Source File Handling (adapted from Clearblue)
//===----------------------------------------------------------------------===//

bool DebugInfoAnalysis::loadSourceFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return false;
    }
    
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        // Trim leading whitespace for better display
        size_t start = line.find_first_not_of(" \t");
        if (start != std::string::npos) {
            lines.push_back(line.substr(start));
        } else {
            lines.push_back("");
        }
    }
    file.close();
    
    sourceFileCache[filepath] = lines;
    return true;
}

std::string DebugInfoAnalysis::findSourceFile(const std::string& filename) {
    if (filename.empty()) {
        return "";
    }
    
    // Use LLVM's file system utilities for better path handling
    if (sys::fs::exists(filename) && !sys::fs::is_directory(filename)) {
        return filename;
    }
    
    // Try various search strategies
    std::vector<std::string> searchPaths;
    
    // 1. Try as absolute path
    if (sys::path::has_root_directory(filename)) {
        searchPaths.push_back(filename);
    } else {
        // 2. Relative paths - try common locations
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != nullptr) {
            std::string cwdStr(cwd);
            
            // Use LLVM path utilities for better cross-platform support
            SmallString<256> buf(cwdStr);
            sys::path::append(buf, filename);
            searchPaths.push_back(buf.str().str());
            
            // Parent directories
            SmallString<256> buf2(cwdStr);
            sys::path::append(buf2, "..");
            sys::path::append(buf2, filename);
            searchPaths.push_back(buf2.str().str());
            
            SmallString<256> buf3(cwdStr);
            sys::path::append(buf3, "..");
            sys::path::append(buf3, "..");
            sys::path::append(buf3, filename);
            searchPaths.push_back(buf3.str().str());
            
            SmallString<256> buf4(cwdStr);
            sys::path::append(buf4, "..");
            sys::path::append(buf4, "..");
            sys::path::append(buf4, "..");
            sys::path::append(buf4, filename);
            searchPaths.push_back(buf4.str().str());
            
            // Common benchmark locations
            SmallString<256> buf5(cwdStr);
            sys::path::append(buf5, "benchmarks");
            sys::path::append(buf5, filename);
            searchPaths.push_back(buf5.str().str());
            
            SmallString<256> buf6(cwdStr);
            sys::path::append(buf6, "..");
            sys::path::append(buf6, "benchmarks");
            sys::path::append(buf6, filename);
            searchPaths.push_back(buf6.str().str());
        }
        
        // Also try relative to current working directory
        searchPaths.push_back(filename);
    }
    
    // Try each path using LLVM file system utilities
    for (const auto& path : searchPaths) {
        if (sys::fs::exists(path) && !sys::fs::is_directory(path)) {
            return path;
        }
    }
    
    return "";  // Not found
}

//===----------------------------------------------------------------------===//
// Source Code Extraction
//===----------------------------------------------------------------------===//

std::string DebugInfoAnalysis::getSourceCodeStatement(const Instruction *I) {
    if (!I) return "";
    
    std::string filepath = getSourceFile(I);
    int line = getSourceLine(I);
    
    if (filepath.empty() || line <= 0) {
        return "";
    }
    
    // Try to find the actual file path
    std::string actualPath = findSourceFile(filepath);
    if (actualPath.empty()) {
        actualPath = filepath;  // Use original if not found
    }
    
    // Check if file exists and is not a directory
    if (!sys::fs::exists(actualPath) || sys::fs::is_directory(actualPath)) {
        return "";
    }
    
    // Check cache first
    auto it = sourceFileCache.find(actualPath);
    if (it == sourceFileCache.end()) {
        // Try efficient single-line read first (inspired by Phasar)
        std::ifstream ifs(actualPath, std::ios::binary);
        if (ifs.is_open()) {
            ifs.seekg(std::ios::beg);
            std::string srcLine;
            for (int i = 0; i < line - 1; ++i) {
                ifs.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                if (ifs.eof()) {
                    ifs.close();
                    sourceFileCache[actualPath] = std::vector<std::string>();
                    return "";
                }
            }
            std::getline(ifs, srcLine);
            ifs.close();
            
            // Cache this single line (we'll cache full file on next access if needed)
            // For now, return the line directly
            if (!srcLine.empty()) {
                // Trim leading whitespace for better display
                size_t start = srcLine.find_first_not_of(" \t");
                if (start != std::string::npos) {
                    return srcLine.substr(start);
                }
                return srcLine;
            }
        }
        
        // Fallback: load entire file into cache
        if (loadSourceFile(actualPath)) {
            it = sourceFileCache.find(actualPath);
        } else {
            // Load failed, cache empty result
            sourceFileCache[actualPath] = std::vector<std::string>();
            return "";
        }
    }
    
    // Get the line from cache
    if (it != sourceFileCache.end() && line > 0 && line <= (int)it->second.size()) {
        return it->second[line - 1];
    }
    
    return "";
}

//===----------------------------------------------------------------------===//
// Debug Info Extraction (adapted for LLVM 14+)
//===----------------------------------------------------------------------===//

std::string DebugInfoAnalysis::getSourceFile(const Value *V) {
    if (!V) return "";
    
    // Try to get DIFile from various sources
    const DIFile *DIF = getDIFileFromIR(V);
    if (!DIF) {
        // Fallback: try DILocation
        if (auto *DILoc = getDILocation(V)) {
            DIF = DILoc->getFile();
        }
    }
    
    if (DIF) {
        auto FileName = DIF->getFilename();
        auto DirName = DIF->getDirectory();
        
        if (FileName.empty()) {
            return "";
        }
        
        // Use LLVM path utilities for better cross-platform support
        if (!DirName.empty() && !sys::path::has_root_directory(FileName)) {
            SmallString<256> Buf;
            sys::path::append(Buf, DirName, FileName);
            return Buf.str().str();
        }
        
        return FileName.str();
    }
    
    // Fallback: try to get from module's source filename
    if (const auto *F = dyn_cast<Function>(V)) {
        return F->getParent()->getSourceFileName();
    }
    if (const auto *Arg = dyn_cast<Argument>(V)) {
        return Arg->getParent()->getParent()->getSourceFileName();
    }
    if (const auto *I = dyn_cast<Instruction>(V)) {
        return I->getFunction()->getParent()->getSourceFileName();
    }
    
    return "";
}

int DebugInfoAnalysis::getSourceLine(const Value *V) {
    if (!V) return 0;
    
    // Try DILocation first (for Arguments and Instructions)
    if (auto *DILoc = getDILocation(V)) {
        return DILoc->getLine();
    }
    
    // Try DISubprogram (for Functions)
    if (const auto *F = dyn_cast<Function>(V)) {
        if (auto *DISubpr = F->getSubprogram()) {
            return DISubpr->getLine();
        }
    }
    
    // Try DIGlobalVariable (for Globals)
    if (auto *DIGV = getDIGlobalVariable(V)) {
        return DIGV->getLine();
    }
    
    return 0;
}

int DebugInfoAnalysis::getSourceColumn(const Value *V) {
    if (!V) return 0;
    
    // Globals and Functions have no column info, only DILocation has column
    if (auto *DILoc = getDILocation(V)) {
        return DILoc->getColumn();
    }
    
    return 0;
}

std::string DebugInfoAnalysis::getSourceLocation(const Instruction *I) {
    if (!I) return "unknown:0";

    const DebugLoc &DL = I->getDebugLoc();
    if (!DL) return "unknown:0";

    unsigned Line = DL.getLine();
    unsigned Col = DL.getCol();

    if (auto *Scope = DL.getScope()) {
        if (isa<DIScope>(Scope)) {
            if (auto *File = cast<DIScope>(Scope)->getFile()) {
                return File->getFilename().str() + ":" + std::to_string(Line) +
                       ":" + std::to_string(Col);
            }
        }
    }

    return "unknown:" + std::to_string(Line);
}

std::string DebugInfoAnalysis::getFunctionName(const Instruction *I) {
    if (!I) return "unknown_function";

    const Function *F = I->getFunction();
    if (!F) return "unknown_function";

    std::string funcName;
    
    // Try to get name from debug info first (real source name)
    if (DISubprogram *Subprogram = F->getSubprogram()) {
        funcName = Subprogram->getName().str();
    } else {
        funcName = F->getName().str();
    }
    
    // Demangle C++ and Rust function names for better readability
    return DemangleUtils::demangleWithCleanup(funcName);
}

std::string DebugInfoAnalysis::getVariableName(const Value *V) {
    if (!V) return "temp_var";

    // Check cache first
    auto it = varNameCache.find(V);
    if (it != varNameCache.end()) {
        return it->second;
    }

    std::string varName;
    
    // Try to get variable name from debug info (improved approach from Phasar)
    // First try DILocalVariable
    if (auto *LocVar = getDILocalVariable(V)) {
        varName = LocVar->getName().str();
    }
    // Then try DIGlobalVariable
    else if (auto *GlobVar = getDIGlobalVariable(V)) {
        varName = GlobVar->getName().str();
    }
    
    // Fallback: scan function for dbg intrinsics (for cases where getDILocalVariable fails)
    if (varName.empty()) {
        if (auto *I = dyn_cast<Instruction>(V)) {
            const Function *F = I->getFunction();
            if (F) {
                for (const auto &BB : *F) {
                    for (const auto &Inst : BB) {
                        // Check dbg.declare
                        if (auto *DbgDeclare = dyn_cast<DbgDeclareInst>(&Inst)) {
                            if (DbgDeclare->getAddress() == V) {
                                if (auto *Var = DbgDeclare->getVariable()) {
                                    varName = Var->getName().str();
                                    break;
                                }
                            }
                        }
                        // Check dbg.value
                        else if (auto *DbgValue = dyn_cast<DbgValueInst>(&Inst)) {
                            if (DbgValue->getValue() == V) {
                                if (auto *Var = DbgValue->getVariable()) {
                                    varName = Var->getName().str();
                                    break;
                                }
                            }
                        }
                    }
                    if (!varName.empty()) break;
                }
            }
        }
    }

    // Fallback to LLVM IR name
    if (varName.empty() && V->hasName()) {
        varName = V->getName().str();
        // Demangle if needed
        varName = DemangleUtils::demangleWithCleanup(varName);
    }

    // For load/store, try to get the pointer operand's name
    if (varName.empty()) {
        if (auto* LI = dyn_cast<LoadInst>(V)) {
            const Value* Ptr = LI->getPointerOperand();
            if (Ptr) {
                // Recursively try to get name from pointer
                std::string ptrName = getVariableName(Ptr);
                if (ptrName != "temp_var") {
                    varName = ptrName;
                } else if (Ptr->hasName()) {
                    varName = Ptr->getName().str();
                }
            }
        } else if (auto* SI = dyn_cast<StoreInst>(V)) {
            const Value* Ptr = SI->getPointerOperand();
            if (Ptr) {
                std::string ptrName = getVariableName(Ptr);
                if (ptrName != "temp_var") {
                    varName = ptrName;
                } else if (Ptr->hasName()) {
                    varName = Ptr->getName().str();
                }
            }
        } else if (auto* CI = dyn_cast<CallInst>(V)) {
            // For call instructions, use the function name
            if (const Function* F = CI->getCalledFunction()) {
                varName = F->getName().str();
            }
        }
    }

    if (varName.empty()) {
        varName = "temp_var";
    }

    // Cache the result
    varNameCache[V] = varName;
    return varName;
}

std::string DebugInfoAnalysis::getTypeName(const Value *V) {
    if (!V) return "unknown_type";

    Type *Ty = V->getType();
    if (!Ty) return "unknown_type";

    std::string TypeStr;
    raw_string_ostream RSO(TypeStr);
    Ty->print(RSO);
    return RSO.str();
}
