#include "Alias/TPA/Transforms/RunPrepass.h"

#include "Alias/TPA/Transforms/ExpandByVal.h"
#include "Alias/TPA/Transforms/ExpandConstantExpr.h"
#include "Alias/TPA/Transforms/ExpandGetElementPtr.h"
#include "Alias/TPA/Transforms/ExpandIndirectBr.h"
#include "Alias/TPA/Transforms/FoldIntToPtr.h"
#include "Alias/TPA/Transforms/GlobalCleanup.h"
#include "Alias/TPA/Transforms/LowerGlobalCtor.h"

#include <llvm/IR/Module.h>

using namespace llvm;

namespace transform {

static void runAllPrepasses(Module &module) {
  ModuleAnalysisManager MAM;
  FunctionAnalysisManager FAM;

  // Run Module-level passes
  GlobalCleanup cleanup;
  cleanup.run(module, MAM);

  ResolveAliases resolver;
  resolver.run(module, MAM);

  LowerGlobalCtorPass lowerCtor;
  lowerCtor.run(module, MAM);

  ExpandIndirectBr expIndirectBr;
  expIndirectBr.run(module, MAM);

  ExpandByValPass expByVal;
  expByVal.run(module, MAM);

  // Run Function-level passes
  ExpandConstantExprPass expConstExpr;
  FoldIntToPtrPass foldIntToPtr;
  ExpandGetElementPtrPass expGEP;

  for (auto &F : module) {
    expConstExpr.run(F, FAM);
    foldIntToPtr.run(F, FAM);
    expGEP.run(F, FAM);
  }
}

void runPrepassOn(Module &module) { runAllPrepasses(module); }

} // namespace transform
