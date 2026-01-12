#include "Alias/TPA/PointerAnalysis/Engine/Initializer.h"

#include "Alias/TPA/Context/Context.h"
#include "Alias/TPA/PointerAnalysis/Engine/GlobalState.h"
#include "Alias/TPA/PointerAnalysis/MemoryModel/MemoryManager.h"
#include "Alias/TPA/PointerAnalysis/MemoryModel/PointerManager.h"
#include "Alias/TPA/PointerAnalysis/Program/SemiSparseProgram.h"
#include "Alias/TPA/PointerAnalysis/Support/Memo.h"

namespace tpa {

ForwardWorkList Initializer::runOnInitState(Store &&initStore) {
  ForwardWorkList workList;

  const auto *entryCtx = context::Context::getGlobalContext();
  const auto *entryCFG = globalState.getSemiSparseProgram().getEntryCFG();
  assert(entryCFG != nullptr);
  const auto *entryNode = entryCFG->getEntryNode();

  // Set up argv
  auto &entryFunc = entryCFG->getFunction();
  if (entryFunc.arg_size() > 1) {
    const auto *argvValue = std::next(entryFunc.arg_begin());
    const auto *argvPtr =
        globalState.getPointerManager().getOrCreatePointer(entryCtx, argvValue);
    const auto *argvObj = globalState.getMemoryManager().allocateArgv(argvValue);
    globalState.getEnv().insert(argvPtr, argvObj);
    initStore.insert(argvObj, argvObj);

    if (entryFunc.arg_size() > 2) {
      const auto *envpValue = std::next(argvValue);
      const auto *envpPtr = globalState.getPointerManager().getOrCreatePointer(
          entryCtx, envpValue);
      const auto *envpObj = globalState.getMemoryManager().allocateEnvp(envpValue);
      globalState.getEnv().insert(envpPtr, envpObj);
      initStore.insert(envpObj, envpObj);
    }
  }

  auto pp = ProgramPoint(entryCtx, entryNode);
  memo.update(pp, std::move(initStore));
  workList.enqueue(pp);

  return workList;
}

} // namespace tpa
