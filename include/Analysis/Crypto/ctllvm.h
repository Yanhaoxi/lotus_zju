/**
 * @file ctllvm.h
 * @brief Constant-Time LLVM Analysis for Side-Channel Detection
 *
 * This file provides the CTPass (Constant-Time Pass) for detecting potential
 * side-channel vulnerabilities in cryptographic implementations. It performs
 * taint analysis to track the flow of sensitive (secret) data and reports
 * any leakage through various channels.
 *
 * Key Features:
 * - Taint tracking for secret data
 * - Information flow analysis
 * - Side-channel leakage detection through:
 *   - Cache timing
 *   - Branch conditions
 *   - Variable timing
 * - Def-use chain analysis
 * - Alias analysis integration
 * - Function inlining for precise analysis
 *
 * @author Zhiyuan Zhang
 * @date 2025
 * @ingroup Crypto
 */

/*
 * Copyright 2025 Zhiyuan Zhang
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or, software
 * agreed to in writing distributed under the License is distributed on an "AS
 * IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied. See the License for the specific language governing permissions and
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

// Configuration macros for analysis behavior
// These can be adjusted to customize the analysis behavior
//@{

/// File path for analysis (specify when running in different directory)
#define FILE_PATH ""

/// Enable or disable using Type System for alias analysis
#define TYPE_SYSTEM 1

/// Enable or disable testing all parameters
#define TEST_PARAMETER 1

/// Enable or disable may leak analysis
#define ENABLE_MAY_LEAK 1

/// Try to get variable name from debug info
#define TRY_HARD_ON_NAME 1

/// Enable or disable user specify taint/declassify source
#define USER_SPECIFY 0

/// Set 1 to enable soundness mode
#define SOUNDNESS_MODE 1

/// Threshold for alias analysis (avoid excessive precision cost)
#define ALIAS_THRESHOLD 2000

/// Set 1 to enable reporting leakages
#define REPORT_LEAKAGES 1

/// Set 1 to enable time analysis
#define TIME_ANALYSIS 0

//@}

// For debug mode or dump propagation procedure
//@{
#define DEBUG 0
#define PRINT_FUNCTION DEBUG
//@}

// LLVM version compatibility macros
#if LLVM_VERSION_MAJOR > 15
#define FUNC_NAME_STARTS_WITH(name, prefix) (name.starts_with(prefix))
#define FUNC_NAME_ENDS_WITH(name, suffix) (name.ends_with(suffix))
#else
#define FUNC_NAME_STARTS_WITH(name, prefix) (name.startswith(prefix))
#define FUNC_NAME_ENDS_WITH(name, suffix) (name.endswith(suffix))
#endif

/// Set 1 to continue analysis even if function cannot be inlined
#define AUTO_CONTINUE 1

/// Threshold for function inlining
#define INLINE_THRESHOLD 10

// Error codes for analysis failures
//@{
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
//@}

// For LLVM 19+ new debug info format
#define USE_NEW_DEBUG_INFO LLVM_VERSION_MAJOR >= 19
#if USE_NEW_DEBUG_INFO
#include "llvm-c/Core.h"
#include "llvm-c/DebugInfo.h"
#include "llvm-c/Types.h"
#include "llvm/Transforms/Utils/NameAnonGlobals.h"
#endif

/// Check if a type is pointer-like (pointer, array, or struct)
#define check_pointer_type(type)                                               \
  (type->isPointerTy() || type->isArrayTy() || type->isStructTy())

/**
 * @class CTPass
 * @brief Constant-Time Analysis Pass for side-channel detection
 *
 * This pass performs static taint analysis to detect potential information
 * leakage from secret data in cryptographic implementations. It tracks the
 * flow of sensitive data and reports any operations that could leak
 * information through side channels.
 *
 * Usage:
 *   opt -load-pass-plugin=libctllvm.so -ctpass input.ll -o output.bc
 *
 * @note This is a ModulePass compatible with the new PassManager
 * @see https://llvm.org/docs/NewPassManager.html
 */
struct CTPass : public PassInfoMixin<CTPass> {
  // ============================================================================
  // Target Value Specification
  // ============================================================================

  /**
   * @struct target_value_info
   * @brief Information about a value to track in the analysis
   *
   * Specifies which values should be considered as taint sources (secret data)
   * or declassification points.
   */
  struct target_value_info {
    StringRef function_name = "0"; ///< Function containing the value
    StringRef value_name = "0";    ///< Name of the value
    StringRef value_type = "0";    ///< Type of the value
    StringRef field_name = "0";    ///< Field name (for structs)
    int line_number = -1;          ///< Source line number
  };

  // ============================================================================
  // Global Statistics and Tracking
  // ============================================================================

  /// Statistics for functions that cannot be inlined
  std::vector<int> statistics_cannot_inline_cases;

  /// Set of secure function names (known not to leak)
  llvm::SetVector<StringRef> secure_function_names;

  /// User-specified target values to track
  llvm::SetVector<target_value_info *> specify_target_values;

  /// User-specified declassified values
  llvm::SetVector<target_value_info *> specify_declassified_values;

  /// Flag indicating whether taint source was specified
  bool specify_taint_flag = false;

  // Statistics counters
  int statistics_taint_source = 0;        ///< Number of taint sources
  int statistics_secure_taint_source = 0; ///< Secure taint sources
  int statistics_analyzed_functions = 0;  ///< Functions analyzed
  int statistics_too_many_alias = 0;      ///< Too many aliases
  int statistics_overall_functions = 0;   ///< Total functions
  int statistics_secure_functions = 0;    ///< Secure functions
  int statistics_inline_success = 0;      ///< Successful inlines
  int statistics_inline_fail = 0;         ///< Failed inlines
  int statistics_no_constant_size = 0;    ///< Non-constant sizes

  // ============================================================================
  // Taint Tracking Sets
  // ============================================================================

  /// Always clear for each taint source
  llvm::SetVector<Value *> high_values;    ///< Secret (high) values
  llvm::SetVector<Value *> low_values;     ///< Public (low) values
  llvm::SetVector<Value *> high_mayvalues; ///< Possibly secret
  llvm::SetVector<Value *> low_mayvalues;  ///< Possibly public

  // ============================================================================
  // Update Methods
  // ============================================================================

  /**
   * @brief Update the list of secure function names
   *
   * These functions are considered not affecting the soundness of the analysis.
   * That is, they do not change memory content or control flow related to
   * secrets.
   */
  void update_secure_function_names();

  /**
   * @brief Update the list of target values to track
   * @param target_values Values to track as taint sources
   * @param specify_declassified_values Values to declassify
   * @return true if update successful
   */
  bool update_target_values(
      llvm::SetVector<target_value_info *> &target_values,
      llvm::SetVector<target_value_info *> &specify_declassified_values);

  /**
   * @brief Update the taint list based on source code annotations
   * @param M The module being analyzed
   * @param F The current function
   * @param I Current instruction
   * @param declassify_flag Whether this is a declassification
   * @param tainted_values Output set of tainted values
   * @param entries Target value specifications
   * @return true if update successful
   */
  bool update_taint_list(Module &M, Function &F, llvm::Instruction &I,
                         bool declassify_flag,
                         llvm::SetVector<Value *> &tainted_values,
                         const llvm::SetVector<target_value_info *> &entries);

  // ============================================================================
  // Dependency Chain Building
  // ============================================================================

  /**
   * @brief Build the def-use chain for specified values
   * @param specify_target_values Values to track
   * @param declassified_values Values that are declassified
   * @return 0 on success, error code on failure
   */
  int build_dependency_chain(
      llvm::SetVector<Instruction *> &specify_target_values,
      llvm::SetVector<Value *> &declassified_values);

  /**
   * @brief Find all instructions aliased with target instructions
   * @param aliasedInstructions Output: instructions that alias with targets
   * @param taintedInstructions Output: instructions using tainted values
   * @param SorLInstructions Store/Load instructions
   * @param AA Alias analysis results
   * @param Arg The argument/value to check
   * @param declassified_values Declassified values
   * @return Number of aliased instructions found
   */
  int find_aliased_instructions(
      llvm::SetVector<Instruction *> &aliasedInstructions,
      llvm::SetVector<Instruction *> &taintedInstructions,
      llvm::SetVector<Instruction *> &SorLInstructions, AAResults &AA,
      Value *Arg, llvm::SetVector<Value *> &declassified_values);

  // ============================================================================
  // Leakage Analysis and Reporting
  // ============================================================================

  /**
   * @brief Check for information leakage through instructions
   * @param taintedInstructions Instructions using tainted values
   * @param leak_through_cache Cache-based leakage points
   * @param leak_through_branch Branch-based leakage points
   * @param leak_through_variable_timing Variable timing leakage
   * @param Arg The taint source argument
   * @param F The function being analyzed
   * @param FAM Function analysis manager
   */
  void checkInstructionLeaks(
      llvm::SetVector<Instruction *> &taintedInstructions,
      std::map<Instruction *, int> &leak_through_cache,
      std::map<Instruction *, int> &leak_through_branch,
      std::map<Instruction *, int> &leak_through_variable_timing, Value *Arg,
      Function &F, FunctionAnalysisManager &FAM);

  /**
   * @brief Print leakage information
   * @param type Type of leakage
   * @param leakMap Map of leaking instructions
   * @param may_must May-leak or must-leak classification
   * @param taintedInstructions Instructions using tainted values
   */
  void printLeakage(const std::string &type,
                    const std::map<Instruction *, int> &leakMap, int may_must,
                    llvm::SetVector<Instruction *> &taintedInstructions);

  /**
   * @brief Report all detected leakages
   * @param taintedInstructions Instructions using tainted values
   * @param leak_through_cache Cache-based leakage
   * @param leak_through_branch Branch-based leakage
   * @param leak_through_variable_timing Variable timing leakage
   * @param may_must May-leak or must-leak
   */
  void
  report_leakage(llvm::SetVector<Instruction *> &taintedInstructions,
                 std::map<Instruction *, int> &leak_through_cache,
                 std::map<Instruction *, int> &leak_through_branch,
                 std::map<Instruction *, int> &leak_through_variable_timing,
                 int may_must);

  /**
   * @brief Print source code location
   * @param filename Source file name
   * @param line_number Line number
   */
  void print_source_code(std::string filename, int line_number);

  // ============================================================================
  // Type System Support
  // ============================================================================

  /**
   * @brief Wrap metadata around an instruction for type-based taint tracking
   * @param I The instruction to wrap
   * @param Arg The taint source argument
   * @param alias_flag Whether this is due to aliasing
   * @param init_flag Whether this is initialization
   * @param initial_taint_arg The initial taint argument
   * @return true if wrapping successful
   */
  bool wrap_metadata(llvm::Instruction &I, Value *Arg, bool alias_flag,
                     bool init_flag = false,
                     Value *initial_taint_arg = nullptr);

  // ============================================================================
  // Memory Operation Analysis
  // ============================================================================

  /**
   * @brief Check for HIGH values stored through memcpy/memmove
   * @param I The memory operation instruction
   * @param AA Alias analysis results
   * @param SorLInstructions Store/Load instructions
   * @return true if secret data is being transferred
   */
  bool reason_memcpy(llvm::Instruction &I, AliasAnalysis &AA,
                     llvm::SetVector<Instruction *> &SorLInstructions);

  // ============================================================================
  // Helper Functions
  // ============================================================================

  /**
   * @brief Get debug information for a value
   * @tparam T The type of debug info to retrieve
   * @param V The value to get debug info for
   * @param Name Optional name to search for
   * @param F The function containing the value
   * @return The requested debug information
   */
  template <typename T> T getDebugInfo(Value *V, StringRef Name, Function &F);

  /**
   * @brief Get the index of a field in a struct type
   * @param StructTy The struct type
   * @param FieldName Name of the field
   * @param M The module
   * @return Field index, -1 if not found
   */
  int getFieldIndex(StructType *StructTy, StringRef FieldName, const Module &M);

  // ============================================================================
  // Main Analysis Functions
  // ============================================================================

  /**
   * @brief Analyze a function for constant-time violations
   *
   * Set 1st bit to 1 if violation at def_use chain; 2nd bit to 1 if violation
   * at must alias; 3rd bit to 1 if violation at may alias
   *
   * @param F The function to analyze
   * @param FAM Function analysis manager
   * @return Bitmask indicating violation types found
   */
  int Analyze_Function(Function &F, FunctionAnalysisManager &FAM);

  /**
   * @brief Perform def-use chain analysis only
   * @param taintedInstruction Output: instructions using tainted values
   * @param declassified_values Values that are declassified
   */
  void def_use_only(llvm::SetVector<Instruction *> &taintedInstruction,
                    llvm::SetVector<Value *> &declassified_values);

  /**
   * @brief Perform def-use analysis with must-alias checking
   * @param taintedInstructions Instructions using tainted values
   * @param aliasedInstructions Instructions that alias with tainted values
   * @param SorLInstructions Store/Load instructions
   * @param AA Alias analysis results
   * @param Arg The taint source argument
   * @param declassified_values Declassified values
   */
  void def_use_alias(llvm::SetVector<Instruction *> &taintedInstructions,
                     llvm::SetVector<Instruction *> &aliasedInstructions,
                     llvm::SetVector<Instruction *> &SorLInstructions,
                     AAResults &AA, Value *Arg,
                     llvm::SetVector<Value *> &declassified_values);

  /**
   * @brief Perform def-use analysis with may-alias checking
   * @param taintedInstructions Instructions using tainted values
   * @param aliasedInstructions Instructions that may alias with tainted values
   * @param SorLInstructions Store/Load instructions
   * @param AA Alias analysis results
   * @param Arg The taint source argument
   * @param declassified_values Declassified values
   */
  void def_use_may_alias(llvm::SetVector<Instruction *> &taintedInstructions,
                         llvm::SetVector<Instruction *> &aliasedInstructions,
                         llvm::SetVector<Instruction *> &SorLInstructions,
                         AAResults &AA, Value *Arg,
                         llvm::SetVector<Value *> &declassified_values);

  /**
   * @brief Check and report violations for a specific argument
   * @param Arg The argument to check
   * @param F The function containing the argument
   * @param FAM Function analysis manager
   * @param taintedInstructions Instructions using tainted values
   * @param leak_through_cache Cache-based leakage
   * @param leak_through_branch Branch-based leakage
   * @param leak_through_variable_timing Variable timing leakage
   * @param mode Analysis mode
   * @return Error code or violation bitmask
   */
  int check_and_report(
      Value *Arg, Function &F, FunctionAnalysisManager &FAM,
      llvm::SetVector<Instruction *> &taintedInstructions,
      std::map<Instruction *, int> &leak_through_cache,
      std::map<Instruction *, int> &leak_through_branch,
      std::map<Instruction *, int> &leak_through_variable_timing, int mode);

  // ============================================================================
  // Function Inlining
  // ============================================================================

  /**
   * @brief Get all function calls that could be inlined
   * @param F The function to analyze
   * @param functions_to_inline Output: functions that can be inlined
   * @return Number of functions found
   */
  int getFunctionCalls(Function &F, std::set<Function *> &functions_to_inline);

  /**
   * @brief Inline function calls in a function
   * @param F The function to process
   * @param functions_to_inline Functions to inline
   * @return Number of functions inlined
   */
  int inlineFunctionCalls(Function &F,
                          std::set<Function *> &functions_to_inline);

  /**
   * @brief Recursively inline calls within a function
   * @param F The function to process
   * @return The final function after all inlining
   */
  Function *recursive_inline_calls(Function *F);

  /**
   * @brief Print analysis statistics
   */
  void print_statistics();

  // ============================================================================
  // Pass Entry Point
  // ============================================================================

  /**
   * @brief Run the analysis on a module
   * @param M The module to analyze
   * @param MAM Module analysis manager
   * @return Preserved analyses
   */
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);
};

/**
 * @brief Get the pass plugin information
 * @return Plugin info structure for registration
 */
PassPluginLibraryInfo getPassPluginInfo();

/**
 * @brief External symbol for pass registration
 */
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo();

// Helper structs for template specialization (C++11/14 compatible)
// Using tag dispatch pattern
namespace ctllvm_detail {
template <typename T> struct get_debug_info_impl;

template <> struct get_debug_info_impl<StringRef> {
  template <typename DbgVarType>
  static StringRef extract_name(DbgVarType *DbgVar) {
    return DbgVar->getName();
  }
  template <typename DbgType> static StringRef extract_value(DbgType *Dbg) {
    return Dbg->getVariable()->getName();
  }
  template <typename DbgDeclareType>
  static StringRef extract_address(DbgDeclareType * /*DbgDeclare*/) {
    return StringRef("");
  }
  static StringRef extract_instruction(Instruction * /*I*/) {
    return StringRef("");
  }
  static StringRef extract_line(int /*line*/) { return StringRef(""); }
  static StringRef extract_unknown() { return StringRef("Unknown"); }
  static StringRef extract_default() { return StringRef(""); }
};

template <> struct get_debug_info_impl<Value *> {
  template <typename DbgVarType>
  static Value *extract_name(DbgVarType * /*DbgVar*/) {
    return nullptr;
  }
  template <typename DbgType> static Value *extract_value(DbgType *Dbg) {
    return Dbg->getValue();
  }
  template <typename DbgDeclareType>
  static Value *extract_address(DbgDeclareType *DbgDeclare) {
    return DbgDeclare->getAddress();
  }
  static Value *extract_instruction(Instruction *I) { return I; }
  static Value *extract_line(int /*line*/) { return nullptr; }
  static Value *extract_unknown() { return nullptr; }
  static Value *extract_default() { return nullptr; }
};

template <> struct get_debug_info_impl<int> {
  template <typename DbgVarType>
  static int extract_name(DbgVarType * /*DbgVar*/) {
    return -1;
  }
  template <typename DbgType> static int extract_value(DbgType * /*Dbg*/) {
    return -1;
  }
  template <typename DbgDeclareType>
  static int extract_address(DbgDeclareType * /*DbgDeclare*/) {
    return -1;
  }
  static int extract_instruction(Instruction * /*I*/) { return -1; }
  static int extract_line(int line) { return line; }
  static int extract_unknown() { return -1; }
  static int extract_default() { return -1; }
};
} // namespace ctllvm_detail

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
              return ctllvm_detail::get_debug_info_impl<T>::extract_line(
                  DbgLoc.getLine());
            }
          }
        }
      }
#endif
      if (auto *DbgDeclare = dyn_cast<DbgDeclareInst>(&I)) {
        if ((V && DbgDeclare->getAddress() == V) ||
            (!Name.empty() && DbgDeclare->getVariable()->getName() == Name)) {
          if (std::is_same<T, StringRef>::value) {
            return ctllvm_detail::get_debug_info_impl<T>::extract_name(
                DbgDeclare->getVariable());
          } else if (std::is_same<T, Value *>::value) {
            return ctllvm_detail::get_debug_info_impl<T>::extract_address(
                DbgDeclare);
          } else if (std::is_same<T, int>::value) {
            // errs() << "Cannot find the line of the value\n";
            return ctllvm_detail::get_debug_info_impl<T>::extract_line(
                DbgDeclare->getDebugLoc().getLine());
          }
        }
      } else if (auto *DbgValue = dyn_cast<DbgValueInst>(&I)) {
        if ((V && DbgValue->getValue() == V) ||
            (!Name.empty() && DbgValue->getVariable()->getName() == Name)) {
          if (std::is_same<T, StringRef>::value) {
            return ctllvm_detail::get_debug_info_impl<T>::extract_name(
                DbgValue->getVariable());
          } else if (std::is_same<T, Value *>::value) {
            return ctllvm_detail::get_debug_info_impl<T>::extract_value(
                DbgValue);
          } else if (std::is_same<T, int>::value) {
            const auto *DIVar =
                dyn_cast<DILocalVariable>(DbgValue->getVariable());
            if (DIVar)
              return ctllvm_detail::get_debug_info_impl<T>::extract_line(
                  DIVar->getLine());
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
              return ctllvm_detail::get_debug_info_impl<T>::extract_instruction(
                  &I);
            } else if (std::is_same<T, int>::value) {
              if (DebugLoc)
                return ctllvm_detail::get_debug_info_impl<T>::extract_line(
                    DebugLoc.getLine());
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
