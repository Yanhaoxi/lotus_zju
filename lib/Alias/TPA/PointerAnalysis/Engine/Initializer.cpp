#include "Alias/TPA/PointerAnalysis/Engine/Initializer.h"

#include "Alias/TPA/Context/Context.h"
#include "Alias/TPA/PointerAnalysis/Engine/GlobalState.h"
#include "Alias/TPA/PointerAnalysis/MemoryModel/MemoryManager.h"
#include "Alias/TPA/PointerAnalysis/MemoryModel/PointerManager.h"
#include "Alias/TPA/PointerAnalysis/Program/SemiSparseProgram.h"
#include "Alias/TPA/PointerAnalysis/Support/Memo.h"
#include "llvm/Support/raw_ostream.h"

namespace tpa {

ForwardWorkList Initializer::runOnInitState(Store &&initStore) {
  ForwardWorkList workList;

  const auto *entryCtx = context::Context::getGlobalContext();
  const auto *entryCFG = globalState.getSemiSparseProgram().getEntryCFG();
  assert(entryCFG != nullptr);
  const auto *entryNode = entryCFG->getEntryNode();
  if (!entryCFG || !entryNode) {
    llvm::errs() << "TPA initializer error: entry CFG or entry node missing\n";
    return workList;
  }
  llvm::errs() << "TPA initializer: entry CFG=" << entryCFG
               << " entry node=" << entryNode << "\n";

  // Set up argv
  auto &entryFunc = entryCFG->getFunction();
  llvm::errs() << "TPA initializer: entry function=" << entryFunc.getName()
               << " args=" << entryFunc.arg_size() << "\n";
  if (entryFunc.arg_size() > 1) {
    const auto *argvValue = std::next(entryFunc.arg_begin());
    const auto *argvPtr =
        globalState.getPointerManager().getOrCreatePointer(entryCtx, argvValue);
    llvm::errs() << "TPA initializer: argv ptr=" << argvPtr << "\n";
    const auto *argvObj = globalState.getMemoryManager().allocateArgv(argvValue);
    llvm::errs() << "TPA initializer: argv obj=" << argvObj << "\n";
    globalState.getEnv().insert(argvPtr, argvObj);
    initStore.insert(argvObj, argvObj);

    if (entryFunc.arg_size() > 2) {
      const auto *envpValue = std::next(argvValue);
      const auto *envpPtr = globalState.getPointerManager().getOrCreatePointer(
          entryCtx, envpValue);
      llvm::errs() << "TPA initializer: envp ptr=" << envpPtr << "\n";
      const auto *envpObj = globalState.getMemoryManager().allocateEnvp(envpValue);
      llvm::errs() << "TPA initializer: envp obj=" << envpObj << "\n";
      globalState.getEnv().insert(envpPtr, envpObj);
      initStore.insert(envpObj, envpObj);
    }
  }

  auto pp = ProgramPoint(entryCtx, entryNode);
  llvm::errs() << "TPA initializer: initial program point ready\n";
  memo.update(pp, std::move(initStore));
  llvm::errs() << "TPA initializer: memo updated\n";
  workList.enqueue(pp);
  llvm::errs() << "TPA initializer: worklist enqueued\n";

  return workList;
}

} // namespace tpa
