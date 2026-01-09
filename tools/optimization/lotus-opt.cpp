/*
 * lotus-opt
 * A command-line driver for inter-procedural optimizations in lib/Optimization.
 */

#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/InitializePasses.h>
#include <llvm/PassRegistry.h>
#include <llvm/Pass.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Analysis/CallGraph.h>
#include <llvm/Transforms/IPO.h>

#include "Alias/seadsa/InitializePasses.hh"
#include "Alias/seadsa/DsaAnalysis.hh"
#include "Alias/seadsa/AllocSiteInfo.hh"
#include "Alias/seadsa/ShadowMem.hh"
#include "Alias/seadsa/support/RemovePtrToInt.hh"
#include "Alias/seadsa/AllocWrapInfo.hh"
#include "Alias/seadsa/DsaLibFuncInfo.hh"

#include <memory>
#include <string>

using namespace llvm;
using namespace seadsa;

namespace {

static cl::OptionCategory OptCat("Lotus Optimization Tool");

static cl::opt<std::string> InputFilename(
    cl::Positional, cl::desc("<input bitcode file>"), cl::Required,
    cl::cat(OptCat));

static cl::opt<std::string> OutputFilename(
    "o", cl::desc("Override output filename (default: -)"),
    cl::value_desc("filename"), cl::init("-"), cl::cat(OptCat));

static cl::opt<bool> OutputAssembly(
    "S", cl::desc("Write LLVM assembly instead of bitcode"),
    cl::init(false), cl::cat(OptCat));

static cl::opt<bool> EnableAllIP(
    "ip-all", cl::desc("Enable all inter-procedural optimizations"),
    cl::init(false), cl::cat(OptCat));

static cl::opt<bool> EnableAInline(
    "ainline", cl::desc("Run aggressive inliner"),
    cl::init(false), cl::cat(OptCat));

static cl::opt<bool> EnableIPDSE(
    "ipdse", cl::desc("Run inter-procedural dead store elimination"),
    cl::init(false), cl::cat(OptCat));

static cl::opt<bool> EnableIPRLE(
    "ip-rle", cl::desc("Run inter-procedural redundant load elimination"),
    cl::init(false), cl::cat(OptCat));

static cl::opt<bool> EnableIPSink(
    "ip-sink", cl::desc("Run inter-procedural store sinking"),
    cl::init(false), cl::cat(OptCat));

static cl::opt<bool> EnableIPForward(
    "ip-forward", cl::desc("Run inter-procedural store-to-load forwarding"),
    cl::init(false), cl::cat(OptCat));

static bool addPassByName(legacy::PassManager &PM, StringRef PassName) {
  const PassRegistry &Registry = *PassRegistry::getPassRegistry();
  const PassInfo *PI = Registry.getPassInfo(PassName);
  if (!PI) {
    errs() << "error: unknown pass '" << PassName << "'\n";
    return false;
  }
  PM.add(PI->createPass());
  return true;
}

} // namespace

int main(int argc, char **argv) {
  InitLLVM X(argc, argv);

  PassRegistry &Registry = *PassRegistry::getPassRegistry();
  initializeCore(Registry);
  initializeAnalysis(Registry);
  initializeTransformUtils(Registry);
  initializeIPO(Registry);
  initializeCallGraphWrapperPassPass(Registry);
  initializeGlobalsAAWrapperPassPass(Registry);
  initializeTargetLibraryInfoWrapperPassPass(Registry);
  initializeDominatorTreeWrapperPassPass(Registry);
  initializeAssumptionCacheTrackerPass(Registry);
  
  // Initialize SeaDsa passes
  initializeAnalysisPasses(Registry);
  initializeRemovePtrToIntPass(Registry);
  initializeAllocWrapInfoPass(Registry);
  initializeDsaLibFuncInfoPass(Registry);
  initializeAllocSiteInfoPass(Registry);
  initializeDsaAnalysisPass(Registry);
  initializeShadowMemPassPass(Registry);

  cl::ParseCommandLineOptions(
      argc, argv,
      "Lotus optimization tool for inter-procedural passes\n");

  if (EnableAllIP) {
    EnableAInline = true;
    EnableIPDSE = true;
    EnableIPRLE = true;
    EnableIPSink = true;
    EnableIPForward = true;
  }

  if (!EnableAInline && !EnableIPDSE && !EnableIPRLE && !EnableIPSink &&
      !EnableIPForward) {
    errs() << "error: no optimization selected; use -ip-all or specific flags\n";
    return 1;
  }

  LLVMContext Context;
  SMDiagnostic Err;
  std::unique_ptr<Module> M = parseIRFile(InputFilename, Err, Context);
  if (!M) {
    Err.print(argv[0], errs());
    return 1;
  }

  if (verifyModule(*M, &errs())) {
    errs() << "error: module verification failed\n";
    return 1;
  }

  std::unique_ptr<ToolOutputFile> Out;
  if (!OutputFilename.empty() && OutputFilename != "-") {
    std::error_code EC;
    Out = std::make_unique<ToolOutputFile>(OutputFilename, EC, sys::fs::OF_None);
    if (EC) {
      errs() << EC.message() << '\n';
      return 1;
    }
  }

  legacy::PassManager PM;
  bool Ok = true;

  // Check if any IP optimization that requires MemorySSA is enabled
  bool needsMemorySSA =
      EnableIPDSE || EnableIPRLE || EnableIPSink || EnableIPForward;

  // Run aggressive inliner before ShadowMem to avoid breaking
  // shadow.mem/store adjacency assumptions.
  if (EnableAInline)
    Ok &= addPassByName(PM, "ainline");

  // Add prerequisite passes for MemorySSA-based optimizations
  if (needsMemorySSA) {
    // SeaDsa prerequisite passes - must be added in order
    // These passes set up the analysis infrastructure needed by ShadowMem
    PM.add(new RemovePtrToInt());
    PM.add(new AllocWrapInfo());
    PM.add(new DsaLibFuncInfo());
    PM.add(new AllocSiteInfo());
    PM.add(new DsaAnalysis());

    // ShadowMem pass to instrument code with MemorySSA
    // This pass requires all the above passes to have run first
    PM.add(createShadowMemPass());
  }

  // Run MemorySSA-based IP optimizations
  if (EnableIPDSE)
    Ok &= addPassByName(PM, "ipdse");
  if (EnableIPRLE)
    Ok &= addPassByName(PM, "ip-rle");
  if (EnableIPSink)
    Ok &= addPassByName(PM, "ip-sink");
  if (EnableIPForward)
    Ok &= addPassByName(PM, "ip-forward");

  if (!Ok)
    return 1;

  PM.run(*M);

  raw_ostream &OS = Out ? Out->os() : outs();
  if (OutputAssembly) {
    M->print(OS, nullptr);
  } else {
    WriteBitcodeToFile(*M, OS);
  }

  if (Out)
    Out->keep();

  return 0;
}
