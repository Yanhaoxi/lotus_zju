#include "Alias/TPA/PointerAnalysis/Engine/Initializer.h"

#include "Alias/TPA/Context/Context.h"
#include "Alias/TPA/PointerAnalysis/Engine/GlobalState.h"
#include "Alias/TPA/PointerAnalysis/MemoryModel/MemoryManager.h"
#include "Alias/TPA/PointerAnalysis/MemoryModel/PointerManager.h"
#include "Alias/TPA/PointerAnalysis/Program/SemiSparseProgram.h"
#include "Alias/TPA/PointerAnalysis/Support/Memo.h"
#include "Alias/TPA/Util/Log.h"
#include "llvm/Support/raw_ostream.h"

namespace tpa {

ForwardWorkList Initializer::runOnInitState(Store &&initStore) {
  ForwardWorkList workList;

  const auto *entryCtx = context::Context::getGlobalContext();
  const auto *entryCFG = globalState.getSemiSparseProgram().getEntryCFG();
  assert(entryCFG != nullptr);
  const auto *entryNode = entryCFG->getEntryNode();
  if (!entryCFG || !entryNode) {
    LOG_ERROR("TPA initializer error: entry CFG or entry node missing");
    return workList;
  }
  LOG_DEBUG("TPA initializer: entry CFG={} entry node={}", 
            static_cast<const void*>(entryCFG), static_cast<const void*>(entryNode));

  // Set up argv
  auto &entryFunc = entryCFG->getFunction();
  LOG_DEBUG("TPA initializer: entry function={} args={}", 
            entryFunc.getName().str(), entryFunc.arg_size());
  if (entryFunc.arg_size() > 1) {
    const auto *argvValue = std::next(entryFunc.arg_begin());
    const auto *argvPtr =
        globalState.getPointerManager().getOrCreatePointer(entryCtx, argvValue);
    LOG_DEBUG("TPA initializer: argv ptr={}", static_cast<const void*>(argvPtr));
    const auto *argvObj = globalState.getMemoryManager().allocateArgv(argvValue);
    LOG_DEBUG("TPA initializer: argv obj={}", static_cast<const void*>(argvObj));
    globalState.getEnv().insert(argvPtr, argvObj);
    initStore.insert(argvObj, argvObj);

    if (entryFunc.arg_size() > 2) {
      const auto *envpValue = std::next(argvValue);
      const auto *envpPtr = globalState.getPointerManager().getOrCreatePointer(
          entryCtx, envpValue);
      LOG_DEBUG("TPA initializer: envp ptr={}", static_cast<const void*>(envpPtr));
      const auto *envpObj = globalState.getMemoryManager().allocateEnvp(envpValue);
      LOG_DEBUG("TPA initializer: envp obj={}", static_cast<const void*>(envpObj));
      globalState.getEnv().insert(envpPtr, envpObj);
      initStore.insert(envpObj, envpObj);
    }
  }

  auto pp = ProgramPoint(entryCtx, entryNode);
  LOG_DEBUG("TPA initializer: initial program point ready");
  memo.update(pp, std::move(initStore));
  LOG_DEBUG("TPA initializer: memo updated");
  workList.enqueue(pp);
  LOG_DEBUG("TPA initializer: worklist enqueued");

  return workList;
}

} // namespace tpa
