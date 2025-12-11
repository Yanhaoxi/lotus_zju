/*
 * Copyright 2025 Zhiyuan Zhang
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#define LLVM_ENABLE_DUMP

#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Value.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

#include <map>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

using namespace llvm;

// We need to specify the path when the analysis is done in a directory
// different from the compilation directory
#define FILE_PATH ""

#define TYPE_SYSTEM 1      // Enable or disable using Type System
#define TEST_PARAMETER 1   // Enable or disable testing all parameters
#define ENABLE_MAY_LEAK 1  // Enable or disable may leak analysis
#define TRY_HARD_ON_NAME 1 // Try to get variable name from debug info
#define USER_SPECIFY 0 // Enable or disable user specify taint/declassify source

#define SOUNDNESS_MODE 1     // Set 1 to enable soundness mode
#define ALIAS_THRESHOLD 2000 // 100000 // Set the threshold for alias analysis
#define REPORT_LEAKAGES 1    // Set 1 to enable reporting leakages
#define TIME_ANALYSIS 0      // Set 1 to enable time analysis

// For debug mode or dump propagation procedure
#define DEBUG 0
#define PRINT_FUNCTION DEBUG // Print the IR of Function

#if LLVM_VERSION_MAJOR > 15
#define FUNC_NAME_STARTS_WITH(name, prefix) (name.starts_with(prefix))
#define FUNC_NAME_ENDS_WITH(name, suffix) (name.ends_with(suffix))
#else
#define FUNC_NAME_STARTS_WITH(name, prefix) (name.startswith(prefix))
#define FUNC_NAME_ENDS_WITH(name, suffix) (name.endswith(suffix))
#endif

// Collect statistics for functions that can be soundly verified
#define AUTO_CONTINUE 1 // Set 1 to continue the analysis even if the function cannot be inlined
#define INLINE_THRESHOLD 10 // 1000
#define IS_ERROR_CODE(a) a < 0 ? true : false
#define ERROR_CODE_INLINE_ASSEMBLY -1
#define ERROR_CODE_INDIRECT_CALL -2
#define ERROR_CODE_NO_IMPLEMENTATION -3
#define ERROR_CODE_INVOKE_FUNCTION -4
#define ERROR_CODE_INLINE_ITSELF -5
#define ERROR_CODE_INLINE_FAIL -6
#define ERROR_CODE_NOT_CALLBASE -7
#define ERROR_CODE_OVER_THRESHOLD -8
#define ERROR_CODE_TOO_MANY_ALIAS -9
#define ERROR_CODE_NO_CONSTANT_SIZE -10

// Stating from LLVM 19, a new debug info format is used
#define USE_NEW_DEBUG_INFO LLVM_VERSION_MAJOR >= 19
#if USE_NEW_DEBUG_INFO
#include "llvm-c/Core.h"
#include "llvm-c/DebugInfo.h"
#include "llvm-c/Types.h"
#include "llvm/Transforms/Utils/NameAnonGlobals.h"
#endif

#define check_pointer_type(type)                                               \
  (type->isPointerTy() || type->isArrayTy() || type->isStructTy())

struct CTPass : public PassInfoMixin<CTPass> {
  // Specify which value to track
  struct target_value_info {
    StringRef function_name = "0";
    StringRef value_name = "0";
    StringRef value_type = "0";
    StringRef field_name = "0";
    int line_number = -1;
  };

  // Global Lists
  std::vector<int> statistics_cannot_inline_cases;
  llvm::SetVector<StringRef> secure_function_names;
  llvm::SetVector<target_value_info *> specify_target_values;
  llvm::SetVector<target_value_info *> specify_declassified_values;
  bool specify_taint_flag = false;
  int statistics_taint_source = 0;
  int statistics_secure_taint_source = 0;
  int statistics_analyzed_functions = 0;
  int statistics_too_many_alias = 0;
  int statistics_overall_functions = 0;
  int statistics_secure_functions = 0;
  int statistics_inline_success = 0;
  int statistics_inline_fail = 0;
  int statistics_no_constant_size = 0;

  // Always clear it for each taint source
  llvm::SetVector<Value *> high_values;
  llvm::SetVector<Value *> low_values;
  llvm::SetVector<Value *> high_mayvalues;
  llvm::SetVector<Value *> low_mayvalues;

  // These functions are considered not affecting the soundness of the analysis.
  // That is, they do not change memory content or control flow that are related
  // to secrets
  void update_secure_function_names();

  // Specify which value to track
  bool update_target_values(
      llvm::SetVector<target_value_info *> &target_values,
      llvm::SetVector<target_value_info *> &specify_declassified_values);
  bool update_taint_list(Module &M, Function &F, llvm::Instruction &I,
                         bool declassify_flag,
                         llvm::SetVector<Value *> &tainted_values,
                         const llvm::SetVector<target_value_info *> &entries);

  // Update Def-Use Chain and alias analysis
  int build_dependency_chain(
      llvm::SetVector<Instruction *> &specify_target_values,
      llvm::SetVector<Value *> &declassified_values);
  int find_aliased_instructions(
      llvm::SetVector<Instruction *> &aliasedInstructions,
      llvm::SetVector<Instruction *> &taintedInstructions,
      llvm::SetVector<Instruction *> &SorLInstructions, AAResults &AA,
      Value *Arg, llvm::SetVector<Value *> &declassified_values);

  // Analysis and Report
  void checkInstructionLeaks(
      llvm::SetVector<Instruction *> &taintedInstructions,
      std::map<Instruction *, int> &leak_through_cache,
      std::map<Instruction *, int> &leak_through_branch,
      std::map<Instruction *, int> &leak_through_variable_timing, Value *Arg,
      Function &F, FunctionAnalysisManager &FAM);
  void printLeakage(const std::string &type,
                    const std::map<Instruction *, int> &leakMap, int may_must,
                    llvm::SetVector<Instruction *> &taintedInstructions);
  void
  report_leakage(llvm::SetVector<Instruction *> &taintedInstructions,
                 std::map<Instruction *, int> &leak_through_cache,
                 std::map<Instruction *, int> &leak_through_branch,
                 std::map<Instruction *, int> &leak_through_variable_timing,
                 int may_must);
  void print_source_code(std::string filename, int line_number);

  // For Type System
  bool wrap_metadata(llvm::Instruction &I, Value *Arg, bool alias_flag,
                     bool init_flag = false,
                     Value *initial_taint_arg = nullptr);

  // We want to know if there is any HIGH values have been stored to
  // the src address of a memcpy or memmove
  bool reason_memcpy(llvm::Instruction &I, AliasAnalysis &AA,
                     llvm::SetVector<Instruction *> &SorLInstructions);

  // Help functions
  template <typename T> T getDebugInfo(Value *V, StringRef Name, Function &F);
  int getFieldIndex(StructType *StructTy, StringRef FieldName, const Module &M);

  // Functionality Wrap
  // Set 1st bit to 1 if vioalation at def_use chain; 2nd bit to 1 if violation
  // at must alias; 3rd bit to 1 if violation at may alias
  int Analyze_Function(Function &F, FunctionAnalysisManager &FAM);
  void def_use_only(llvm::SetVector<Instruction *> &taintedInstruction,
                    llvm::SetVector<Value *> &declassified_values);
  void def_use_alias(llvm::SetVector<Instruction *> &taintedInstructions,
                     llvm::SetVector<Instruction *> &aliasedInstructions,
                     llvm::SetVector<Instruction *> &SorLInstructions,
                     AAResults &AA, Value *Arg,
                     llvm::SetVector<Value *> &declassified_values);
  void def_use_may_alias(
      llvm::SetVector<Instruction *> &taintedInstructions,
      llvm::SetVector<Instruction *> &aliasedInstructions,
      llvm::SetVector<Instruction *> &SorLInstructions, AAResults &AA, Value *Arg,
      llvm::SetVector<Value *> &declassified_values);
  int check_and_report(
      Value *Arg, Function &F, FunctionAnalysisManager &FAM,
      llvm::SetVector<Instruction *> &taintedInstructions,
      std::map<Instruction *, int> &leak_through_cache,
      std::map<Instruction *, int> &leak_through_branch,
      std::map<Instruction *, int> &leak_through_variable_timing, int mode);

  // For Function Inlining
  int getFunctionCalls(Function &F, std::set<Function *> &functions_to_inline);
  int inlineFunctionCalls(Function &F,
                          std::set<Function *> &functions_to_inline);
  Function *recursive_inline_calls(Function *F);
  void print_statistics();

  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);
};

PassPluginLibraryInfo getPassPluginInfo();
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo();

// Helper structs for template specialization (C++11/14 compatible)
// Using tag dispatch pattern
namespace ctllvm_detail {
  template <typename T> struct get_debug_info_impl;
  
  template <>
  struct get_debug_info_impl<StringRef> {
    template <typename DbgVarType>
    static StringRef extract_name(DbgVarType *DbgVar) {
      return DbgVar->getName();
    }
    template <typename DbgType>
    static StringRef extract_value(DbgType *Dbg) {
      return Dbg->getVariable()->getName();
    }
    template <typename DbgDeclareType>
    static StringRef extract_address(DbgDeclareType * /*DbgDeclare*/) {
      return StringRef("");
    }
    static StringRef extract_instruction(Instruction * /*I*/) {
      return StringRef("");
    }
    static StringRef extract_line(int /*line*/) {
      return StringRef("");
    }
    static StringRef extract_unknown() {
      return StringRef("Unknown");
    }
    static StringRef extract_default() {
      return StringRef("");
    }
  };
  
  template <>
  struct get_debug_info_impl<Value *> {
    template <typename DbgVarType>
    static Value *extract_name(DbgVarType * /*DbgVar*/) {
      return nullptr;
    }
    template <typename DbgType>
    static Value *extract_value(DbgType *Dbg) {
      return Dbg->getValue();
    }
    template <typename DbgDeclareType>
    static Value *extract_address(DbgDeclareType *DbgDeclare) {
      return DbgDeclare->getAddress();
    }
    static Value *extract_instruction(Instruction *I) {
      return I;
    }
    static Value *extract_line(int /*line*/) {
      return nullptr;
    }
    static Value *extract_unknown() {
      return nullptr;
    }
    static Value *extract_default() {
      return nullptr;
    }
  };
  
  template <>
  struct get_debug_info_impl<int> {
    template <typename DbgVarType>
    static int extract_name(DbgVarType * /*DbgVar*/) {
      return -1;
    }
    template <typename DbgType>
    static int extract_value(DbgType * /*Dbg*/) {
      return -1;
    }
    template <typename DbgDeclareType>
    static int extract_address(DbgDeclareType * /*DbgDeclare*/) {
      return -1;
    }
    static int extract_instruction(Instruction * /*I*/) {
      return -1;
    }
    static int extract_line(int line) {
      return line;
    }
    static int extract_unknown() {
      return -1;
    }
    static int extract_default() {
      return -1;
    }
  };
}

// Template must be visible to callers.
template <typename T>
T CTPass::getDebugInfo(Value *V, StringRef Name, Function &F) {
  for (auto &BB : F) {
    for (auto &I : BB) {
#if USE_NEW_DEBUG_INFO
      // Handle the new debug info format
      for (DbgRecord &DR : I.getDbgRecordRange()) {
        if (auto *Dbg = dyn_cast<DbgVariableRecord>(&DR)) {
          auto *DbgVar = Dbg->getVariable();
          auto DbgLoc = DR.getDebugLoc();
          if ((V && Dbg->getValue() == V) ||
              (!Name.empty() && DbgVar->getName() == Name)) {
            if (std::is_same<T, StringRef>::value) {
              return ctllvm_detail::get_debug_info_impl<T>::extract_value(Dbg);
            } else if (std::is_same<T, Value *>::value) {
              return ctllvm_detail::get_debug_info_impl<T>::extract_value(Dbg);
            } else if (std::is_same<T, int>::value) {
              return ctllvm_detail::get_debug_info_impl<T>::extract_line(DbgLoc.getLine());
            }
          }
        }
      }
#endif
      if (auto *DbgDeclare = dyn_cast<DbgDeclareInst>(&I)) {
        if ((V && DbgDeclare->getAddress() == V) ||
            (!Name.empty() && DbgDeclare->getVariable()->getName() == Name)) {
          if (std::is_same<T, StringRef>::value) {
            return ctllvm_detail::get_debug_info_impl<T>::extract_name(DbgDeclare->getVariable());
          } else if (std::is_same<T, Value *>::value) {
            return ctllvm_detail::get_debug_info_impl<T>::extract_address(DbgDeclare);
          } else if (std::is_same<T, int>::value) {
            // errs() << "Cannot find the line of the value\n";
            return ctllvm_detail::get_debug_info_impl<T>::extract_line(DbgDeclare->getDebugLoc().getLine());
          }
        }
      } else if (auto *DbgValue = dyn_cast<DbgValueInst>(&I)) {
        if ((V && DbgValue->getValue() == V) ||
            (!Name.empty() && DbgValue->getVariable()->getName() == Name)) {
          if (std::is_same<T, StringRef>::value) {
            return ctllvm_detail::get_debug_info_impl<T>::extract_name(DbgValue->getVariable());
          } else if (std::is_same<T, Value *>::value) {
            return ctllvm_detail::get_debug_info_impl<T>::extract_value(DbgValue);
          } else if (std::is_same<T, int>::value) {
            const auto *DIVar =
                dyn_cast<DILocalVariable>(DbgValue->getVariable());
            if (DIVar)
              return ctllvm_detail::get_debug_info_impl<T>::extract_line(DIVar->getLine());
            // errs() << "Cannot find the line of the value\n";
          }
        }
      } else {
        if (I.hasMetadata() && I.getMetadata("dbg")) {
          if ((V && &I == V) || !Name.empty()) {
            auto DebugLoc = I.getDebugLoc();
            if (std::is_same<T, StringRef>::value) {
              // errs() << "Instruction with debug metadata does not have a
              // name.\n";
              return ctllvm_detail::get_debug_info_impl<T>::extract_unknown();
            } else if (std::is_same<T, Value *>::value) {
              return ctllvm_detail::get_debug_info_impl<T>::extract_instruction(&I);
            } else if (std::is_same<T, int>::value) {
              if (DebugLoc)
                return ctllvm_detail::get_debug_info_impl<T>::extract_line(DebugLoc.getLine());
              // errs() << "Cannot find the line of the instruction.\n";
            }
          }
        }
      }
    }
  }

  // Handle default return values for each type
  return ctllvm_detail::get_debug_info_impl<T>::extract_default();
}
