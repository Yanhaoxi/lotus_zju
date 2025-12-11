/*
 * Inlining and statistics helpers for the ctllvm pass.
 */

#include "Analysis/Crypto/ctllvm.h"

#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include <cassert>

using namespace llvm;

void CTPass::update_secure_function_names() {
  secure_function_names.insert("fprintf");
  secure_function_names.insert("fopen");
  secure_function_names.insert("fputc");
  secure_function_names.insert("malloc");
  secure_function_names.insert("calloc");
  secure_function_names.insert("memset");
  secure_function_names.insert("free");
  secure_function_names.insert("explicit_bzero");
  secure_function_names.insert("abort");
  secure_function_names.insert("exit");
}

// int CTPass::get_function_calls
int CTPass::getFunctionCalls(Function &F,
                             std::set<Function *> &functions_to_inline) {
  for (auto &I : instructions(F)) {
    if (auto *CI = dyn_cast<CallInst>(&I)) {
      Function *Callee = CI->getCalledFunction();
      if (Callee && !Callee->isDeclaration()) {
        functions_to_inline.insert(Callee);
        continue;
      }

      if (CI->isInlineAsm())
#if !AUTO_CONTINUE
        return ERROR_CODE_INLINE_ASSEMBLY;
#else
        continue;
#endif
      if (!Callee)
#if !AUTO_CONTINUE
        return ERROR_CODE_INDIRECT_CALL;
#else
        continue;
#endif

      if (Callee->isIntrinsic())
        continue;
#if !AUTO_CONTINUE
      if (!secure_function_names.count(Callee->getName())) {
        errs() << "No implementation for function: " << Callee->getName()
               << "\n";
        return ERROR_CODE_NO_IMPLEMENTATION;
      }
#endif
    }
  }

  return functions_to_inline.size();
}

int CTPass::inlineFunctionCalls(Function &F,
                                std::set<Function *> &functions_to_inline) {
  int num_functions_to_inline = getFunctionCalls(F, functions_to_inline);

#if !AUTO_CONTINUE
  if (IS_ERROR_CODE(num_functions_to_inline))
    return num_functions_to_inline;
#endif

  StringRef funcName = FUNC_NAME_ENDS_WITH(F.getName(), "_ctcloned")
                           ? F.getName().drop_back(9)
                           : F.getName();

  for (Function *F : functions_to_inline) {
    if (funcName == F->getName())
#if !AUTO_CONTINUE
      return ERROR_CODE_INLINE_ITSELF;
#else
      continue;
#endif

    CallBase *CB = dyn_cast<CallBase>(F->user_back());
    InlineFunctionInfo IFI;
    if (!CB)
#if !AUTO_CONTINUE
      return ERROR_CODE_NOT_CALLBASE;
#else
      continue;
#endif
    InlineResult IR = InlineFunction(*CB, IFI);
#if !AUTO_CONTINUE
    if (!IR.isSuccess())
      return ERROR_CODE_INLINE_FAIL;
#endif
  }

  num_functions_to_inline = getFunctionCalls(F, functions_to_inline);
  return num_functions_to_inline;
}

Function *CTPass::recursive_inline_calls(Function *targetFunction) {
  ValueToValueMapTy VMap;
  std::set<Function *> functions_to_inline;
  Function *ClonedFunction = CloneFunction(targetFunction, VMap);
  ClonedFunction->setName(targetFunction->getName() + "_ctcloned");
  int inline_done = -1;
  int inline_counter = 0;
  while (inline_done != 0) {
    std::set<Function *> functions_to_inline;
    inline_done = inlineFunctionCalls(*ClonedFunction, functions_to_inline);
    if (IS_ERROR_CODE(inline_done)) {
      statistics_cannot_inline_cases.push_back(inline_done);
      ClonedFunction->eraseFromParent();
      return nullptr;
    }
    inline_counter++;
    if (inline_counter > INLINE_THRESHOLD) {
      statistics_cannot_inline_cases.push_back(ERROR_CODE_OVER_THRESHOLD);
      ClonedFunction->eraseFromParent();
      return nullptr;
    }
  }

  assert(inline_done == 0 && "Inline function failed");
  return ClonedFunction;
}

void CTPass::print_statistics() {
  // Collect statistics
  int inline_fail = 0;
  int inline_itself = 0;
  int inline_assembly = 0;
  int indirect_call = 0;
  int no_implementation = 0;
  int invoke_function = 0;
  int not_callbase = 0;
  int over_threshold = 0;
  int llvm_memcpy = 0;

  errs() << "===========REPORTING Analysis Overivew=============\n";
  errs() << "Number of overall functions: " << statistics_overall_functions
         << "\n";
  errs() << "Number of analyzed functions: " << statistics_analyzed_functions
         << "\n";
  errs() << "Number of no constant size memcpy: " << statistics_no_constant_size
         << "\n";
  errs() << "Number of too many alias: " << statistics_too_many_alias << "\n";
  errs() << "Number of secure functions: " << statistics_secure_functions
         << "\n";
  errs() << "Number of analyzed taint sources: " << statistics_taint_source
         << "\n";
  errs() << "Number of secure taint sources: " << statistics_secure_taint_source
         << "\n";
  errs() << "==================================================\n";

#if SOUNDNESS_MODE
  for (int code : statistics_cannot_inline_cases) {
    switch (code) {
    case ERROR_CODE_INLINE_FAIL:
      inline_fail++;
      break;
    case ERROR_CODE_INLINE_ITSELF:
      inline_itself++;
      break;
    case ERROR_CODE_INLINE_ASSEMBLY:
      inline_assembly++;
      break;
    case ERROR_CODE_INDIRECT_CALL:
      indirect_call++;
      break;
    case ERROR_CODE_NO_IMPLEMENTATION:
      no_implementation++;
      break;
    case ERROR_CODE_INVOKE_FUNCTION:
      invoke_function++;
      break;
    case ERROR_CODE_NOT_CALLBASE:
      not_callbase++;
      break;
    case ERROR_CODE_OVER_THRESHOLD:
      over_threshold++;
      break;
    }
  }

  errs() << "===========REPORTING INLINE STATISTIC=============\n";
  errs() << "Number of Success inline: " << statistics_inline_success << "\n";
  errs() << "Number of Over Threshold: " << over_threshold << "\n";
  errs() << "Number of inline fail: " << inline_fail << "\n";
  errs() << "Number of inline itself: " << inline_itself << "\n";
  errs() << "Number of inline assembly: " << inline_assembly << "\n";
  errs() << "Number of indirect call: " << indirect_call << "\n";
  errs() << "Number of no implementation: " << no_implementation << "\n";
  errs() << "Number of invoke function: " << invoke_function << "\n";
  errs() << "Number of not callbase: " << not_callbase << "\n";
  errs() << "==================================================\n";
#endif
}
