// Sprattus check helpers: assertion and memory-safety reporting
#include "Analysis/Sprattus/Core/Checks.h"
#include "Analysis/Sprattus/Analyzers/Analyzer.h"
#include "Analysis/Sprattus/Utils/PrettyPrinter.h"
#include "Analysis/Sprattus/Domains/MemRegions.h"
#include "Analysis/Sprattus/Core/repr.h"

#include <llvm/IR/Instructions.h>
#include <llvm/Support/raw_ostream.h>

#include <set>
#include <vector>

using namespace llvm;
using namespace sprattus;

int runAssertionCheck(Analyzer* analyzer, Function* targetFunc) {
  int numViolations = 0;
  int numAssertCalls = 0;
  for (auto& bb : *targetFunc) {
    for (auto& instr : bb) {
      if (llvm::isa<llvm::CallInst>(instr)) {
        auto& call = llvm::cast<llvm::CallInst>(instr);
        auto* calledFunc = call.getCalledFunction();
        if (calledFunc) {
          std::string funcName = calledFunc->getName().str();
          // Check for various assertion function names
          if (funcName == "__assert_fail" || 
              funcName == "__assert_rtn" ||
              (funcName.find("assert") != std::string::npos && calledFunc->doesNotReturn())) {
            numAssertCalls++;
            if (!analyzer->at(&bb)->isBottom()) {
              numViolations++;
              PrettyPrinter pp(true);
              analyzer->at(&bb)->prettyPrint(pp);
              outs() << "\nViolated assertion at " << bb.getName().str()
                     << " (function: " << funcName << "). Computed result:\n"
                     << pp.str() << "\n";
            }
          }
        }
      }
    }
  }
  if (VerboseEnable) {
    outs() << "Found " << numAssertCalls << " assertion call(s) in function.\n";
  }
  if (numViolations) {
    outs() << "============================================================"
              "================\n"
           << "  " << numViolations << " violated assertion"
           << ((numViolations == 1) ? "" : "s") << " detected.\n";
  } else {
    if (numAssertCalls == 0) {
      outs() << "No assertion calls found in function.\n";
    } else {
      outs() << "No violated assertions detected (all " << numAssertCalls 
             << " assertion(s) appear safe).\n";
    }
  }
  return (numViolations < 128) ? numViolations : 1;
}

int runMemSafetyCheck(Analyzer* analyzer, Function* targetFunc) {
  int numViolations = 0;
  std::set<std::pair<const llvm::Value*, const llvm::BasicBlock*>> reported;

  for (auto& bb : *targetFunc) {
    bool containsMemOp = false;
    for (auto& inst : bb) {
      if (llvm::isa<llvm::StoreInst>(inst) || llvm::isa<llvm::LoadInst>(inst))
        containsMemOp = true;
    }
    if (!containsMemOp) continue;

    std::vector<const AbstractValue*> vals;
    analyzer->after(&bb)->gatherFlattenedSubcomponents(&vals);

    for (auto& instr : bb) {
      llvm::Value* ptr = nullptr;
      if (auto asStore = llvm::dyn_cast<llvm::StoreInst>(&instr))
        ptr = asStore->getPointerOperand();
      if (auto asLoad = llvm::dyn_cast<llvm::LoadInst>(&instr))
        ptr = asLoad->getPointerOperand();
      if (!ptr) continue;

      bool isValid = false;
      for (auto v : vals) {
        if (auto asVr =
                dynamic_cast<const sprattus::domains::ValidRegion*>(v)) {
          if (asVr->getRepresentedPointer() == ptr && asVr->isValid()) {
            isValid = true;
            break;
          }
        }
      }
      if (!isValid) {
        auto key = std::make_pair(ptr, &bb);
        if (reported.insert(key).second) {
          numViolations++;
          outs() << "Possibly invalid memory access to " << repr(ptr) << " at "
                 << bb.getName().str() << "\n";
        }
      } else {
        outs() << "Definitely valid memory access to " << repr(ptr) << " at "
               << bb.getName().str() << "\n";
      }
    }
  }

  if (numViolations) {
    outs() << "\n"
           << "============================================================"
              "================\n"
           << " " << numViolations << " possibly invalid memory access"
           << ((numViolations == 1) ? "" : "es") << " detected.\n";
  } else {
    outs() << "\n"
           << "============================================================"
              "================\n"
           << "No possibly invalid memory accesses detected.\n";
  }
  return (numViolations < 128) ? numViolations : 1;
}


