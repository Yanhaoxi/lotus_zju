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

#include "Analysis/Crypto/ctllvm.h"

#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/Utils/Mem2Reg.h"
#include "llvm/Transforms/Utils/PromoteMemToReg.h"

#include <chrono>

using namespace llvm;

PreservedAnalyses CTPass::run(Module &M, ModuleAnalysisManager &MAM) {
  auto &FAM =
      MAM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();

  update_secure_function_names();

#if USER_SPECIFY
  specify_taint_flag = update_target_values(specify_target_values,
                                            specify_declassified_values);
#endif

  for (Function &F : M) {
#if TIME_ANALYSIS
    auto start_timing = std::chrono::high_resolution_clock::now();
#endif
    if (!F.isDeclaration())
      statistics_overall_functions++;
    else
      continue;

#if SOUNDNESS_MODE
    Function *ClonedFunction = recursive_inline_calls(&F);
    if (!ClonedFunction)
      statistics_inline_fail++;
    else
      statistics_inline_success++;

    if (!ClonedFunction)
      errs() << "Cannot analyze function: " << F.getName() << "\n";

    if (ClonedFunction && ClonedFunction->use_empty()) {
      int analysis_result = Analyze_Function(*ClonedFunction, FAM);
      if (analysis_result == ERROR_CODE_TOO_MANY_ALIAS)
        statistics_too_many_alias++;
      else if (analysis_result == ERROR_CODE_NO_CONSTANT_SIZE)
        statistics_no_constant_size++;
      ClonedFunction->eraseFromParent();
    }
#else
    Analyze_Function(F, FAM);
#endif
#if TIME_ANALYSIS
    auto end_timing = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end_timing - start_timing);
    // print in json format
    errs() << "{"
           << "\"function\": \"" << F.getName() << "\", "
           << "\"time\": " << duration.count() << "}\n";
#endif
  }

  print_statistics();
  return PreservedAnalyses::all();
}

PassPluginLibraryInfo getPassPluginInfo() {
  const auto callback = [](PassBuilder &PB) {
    PB.registerPipelineStartEPCallback(
        [](ModulePassManager &MPM, OptimizationLevel Level) {
          MPM.addPass(createModuleToFunctionPassAdaptor(PromotePass()));
          return true;
        });

    PB.registerOptimizerLastEPCallback(
        // PB.registerPipelineStartEPCallback(
#if LLVM_VERSION_MAJOR < 20
        [&](ModulePassManager &MPM, OptimizationLevel Level) {
#else
    [&](ModulePassManager &MPM, OptimizationLevel Level, ThinOrFullLTOPhase Phase) {
#endif
          MPM.addPass(CTPass());
          return true;
        });

    // Register a command-line pass name
    PB.registerPipelineParsingCallback(
        [](StringRef Name, ModulePassManager &MPM,
           ArrayRef<PassBuilder::PipelineElement>) {
          if (Name == "ctllvm") {
            MPM.addPass(CTPass());
            return true;
          }
          return false;
        });
  };

  return {LLVM_PLUGIN_API_VERSION, "CTPass", "0.0.1", callback};
}

// Export plugin registration.
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getPassPluginInfo();
}
