#include "Alias/TPA/Context/KLimitContext.h"
#include "Alias/TPA/PointerAnalysis/Analysis/SemiSparsePointerAnalysis.h"
#include "Alias/TPA/PointerAnalysis/FrontEnd/SemiSparseProgramBuilder.h"
#include "Alias/TPA/Transforms/RunPrepass.h"
#include "Alias/TPA/Util/CommandLine/TypedCommandLineParser.h"
#include "Alias/TPA/Util/IO/PointerAnalysis/Printer.h"
#include "Alias/TPA/Util/IO/PointerAnalysis/WriteDotFile.h"
#include "Alias/TPA/Util/IO/WriteIR.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>

#include <cstdlib>
#include <string>
#include <unordered_set>

using namespace llvm;

static std::string findDefaultPointerSpec() {
  // 1) LOTUS_CONFIG_DIR/ptr.spec
  if (const char *envPath = std::getenv("LOTUS_CONFIG_DIR")) {
    SmallString<256> candidate(envPath);
    sys::path::append(candidate, "ptr.spec");
    if (sys::fs::exists(candidate))
      return candidate.str().str();
  }

  // 2) <cwd>/config/ptr.spec
  SmallString<256> cwd;
  if (!sys::fs::current_path(cwd)) {
    SmallString<256> inCwd = cwd;
    sys::path::append(inCwd, "config", "ptr.spec");
    if (sys::fs::exists(inCwd))
      return inCwd.str().str();

    // 3) <parent of cwd>/config/ptr.spec
    SmallString<256> parent = cwd;
    sys::path::remove_filename(parent);
    sys::path::append(parent, "config", "ptr.spec");
    if (sys::fs::exists(parent))
      return parent.str().str();
  }

  // Fallback to relative path for backward compatibility
  return "config/ptr.spec";
}

static void collectCandidatePointerValues(const Module &M,
                                          std::unordered_set<const Value *> &out) {
  for (const GlobalVariable &GV : M.globals())
    out.insert(&GV);

  for (const Function &F : M) {
    for (const Argument &A : F.args())
      if (A.getType()->isPointerTy())
        out.insert(&A);

    for (const BasicBlock &BB : F) {
      for (const Instruction &I : BB) {
        if (I.getType()->isPointerTy())
          out.insert(&I);
      }
    }
  }
}

int main(int argc, char **argv) {
  InitLLVM X(argc, argv);

  StringRef InputFile;
  StringRef ExtPointerTableFile;
  StringRef PrepassOutFile;
  StringRef CFGDotOutDir;
  bool NoPrepass = false;
  bool PrintPts = false;
  bool PrintIndirectCalls = false;
  unsigned KLimit = 0;

  util::TypedCommandLineParser parser(
      "TPA (flow-/context-sensitive semi-sparse pointer analysis) tool");
  parser.addStringPositionalFlag("input", "Input LLVM IR (.bc or .ll)",
                                 InputFile);
  parser.addStringOptionalFlag("ext",
                               "External pointer table file (optional)",
                               ExtPointerTableFile);
  parser.addBooleanOptionalFlag("no-prepass",
                                "Skip TPA IR normalization prepasses",
                                NoPrepass);
  parser.addStringOptionalFlag(
      "prepass-out",
      "Write module after prepass to this file (suffix .ll or .bc)",
      PrepassOutFile);
  parser.addStringOptionalFlag(
      "cfg-dot-dir",
      "Write per-function pointer CFGs as .dot files into this directory",
      CFGDotOutDir);
  parser.addBooleanOptionalFlag(
      "print-pts",
      "Print points-to sets for pointers that were materialized by the analysis",
      PrintPts);
  parser.addBooleanOptionalFlag(
      "print-indirect-calls",
      "Print resolved targets for each indirect call in the module",
      PrintIndirectCalls);
  parser.addUIntOptionalFlag(
      "k-limit",
      "Set k-limit for context-sensitive analysis (0 = context-insensitive, default: 0)",
      KLimit);
  parser.parseCommandLineOptions(argc, argv);

  LLVMContext Context;
  SMDiagnostic Err;
  std::unique_ptr<Module> M = parseIRFile(InputFile, Err, Context);
  if (!M) {
    Err.print(argv[0], errs());
    return 1;
  }

  if (!NoPrepass) {
    transform::runPrepassOn(*M);
  }

  if (!PrepassOutFile.empty()) {
    const bool isText = PrepassOutFile.endswith_insensitive(".ll");
    util::io::writeModuleToFile(*M, PrepassOutFile.data(), isText);
  }

  // Set k-limit for context-sensitive analysis
  context::KLimitContext::setLimit(KLimit);

  // Build semi-sparse program and run analysis
  tpa::SemiSparseProgramBuilder builder;
  tpa::SemiSparseProgram ssProg = builder.runOnModule(*M);

  tpa::SemiSparsePointerAnalysis analysis;
  std::string pointerSpecPath =
      ExtPointerTableFile.empty() ? findDefaultPointerSpec()
                                  : ExtPointerTableFile.str();
  if (!sys::fs::exists(pointerSpecPath)) {
    errs() << "Pointer spec file not found: " << pointerSpecPath << "\n";
    return 1;
  }
  analysis.loadExternalPointerTable(pointerSpecPath.c_str());
  analysis.runOnProgram(ssProg);

  if (!CFGDotOutDir.empty()) {
    std::error_code EC = sys::fs::create_directories(CFGDotOutDir);
    if (EC) {
      errs() << "Failed to create directory " << CFGDotOutDir << ": "
             << EC.message() << "\n";
      return 2;
    }

    for (const tpa::CFG &cfg : ssProg) {
      const auto &F = cfg.getFunction();
      std::string outPath =
          (CFGDotOutDir + "/" + F.getName() + ".dot").str();
      util::io::writeDotFile(outPath.c_str(), cfg);
    }
  }

  if (PrintIndirectCalls) {
    outs() << "=== Indirect call targets ===\n";
    for (const Function &F : *M) {
      if (F.isDeclaration())
        continue;

      for (const BasicBlock &BB : F) {
        for (const Instruction &I : BB) {
          const auto *CB = dyn_cast<CallBase>(&I);
          if (!CB)
            continue;
          if (CB->getCalledFunction() != nullptr)
            continue;

          auto targets = analysis.getCallees(&I);
          outs() << "\n" << F.getName() << ": " << I << "\n";
          outs() << "  targets(" << targets.size() << "): ";
          for (const Function *TF : targets)
            outs() << TF->getName() << " ";
          outs() << "\n";
        }
      }
    }
  }

  if (PrintPts) {
    outs() << "=== Points-to sets ===\n";

    std::unordered_set<const Value *> values;
    collectCandidatePointerValues(*M, values);

    const auto &PM = analysis.getPointerManager();
    for (const Value *V : values) {
      auto ptrs = PM.getPointersWithValue(V->stripPointerCasts());
      if (ptrs.empty())
        continue;

      outs() << "\nValue: ";
      util::io::dumpValue(outs(), *V);
      outs() << "\n";
      for (const tpa::Pointer *P : ptrs) {
        outs() << "  " << *P << " -> " << analysis.getPtsSet(P) << "\n";
      }
    }
  }

  return 0;
}
