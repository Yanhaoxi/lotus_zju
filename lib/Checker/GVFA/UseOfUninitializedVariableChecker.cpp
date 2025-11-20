#include "Checker/GVFA/UseOfUninitializedVariableChecker.h"
#include "Checker/Report/BugReportMgr.h"
#include "Checker/Report/BugReport.h"
#include "Checker/Report/BugTypes.h"
#include "Analysis/GVFA/GlobalValueFlowAnalysis.h"
#include "Checker/GVFA/CheckerUtils.h"

#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;
using namespace CheckerUtils;

//===----------------------------------------------------------------------===//
// Source and Sink Identification
//===----------------------------------------------------------------------===//

void UseOfUninitializedVariableChecker::getSources(Module *M, VulnerabilitySourcesType &Sources) {
    forEachInstruction(M, [&Sources](const Instruction *I) {
        // Uninitialized allocas
        if (auto *AI = dyn_cast<AllocaInst>(I)) {
            bool hasInitialStore = false;
            for (auto *U : AI->users()) {
                if (auto *SI = dyn_cast<StoreInst>(U)) {
                    if (SI->getPointerOperand() == AI && SI->getParent() == AI->getParent()) {
                        hasInitialStore = true;
                        break;
                    }
                }
            }
            if (!hasInitialStore) {
                Sources[{AI, 1}] = 1;
            }
        }
        // Explicit undef
        else if (isa<UndefValue>(I)) {
            Sources[{I, 1}] = 1;
        }
        // Load from potentially uninitialized memory
        else if (auto *LI = dyn_cast<LoadInst>(I)) {
            if (auto *AI = dyn_cast<AllocaInst>(LI->getPointerOperand())) {
                Sources[{LI, 1}] = 1;
            }
        }
    });
}

void UseOfUninitializedVariableChecker::getSinks(Module *M, VulnerabilitySinksType &Sinks) {
    forEachInstruction(M, [&Sinks](const Instruction *I) {
        const Value *UncheckedOp = nullptr;
        
        if (isa<BinaryOperator>(I) || isa<UnaryOperator>(I)) {
            if (I->getNumOperands() > 0) UncheckedOp = I->getOperand(0);
        }
        else if (auto *Cmp = dyn_cast<CmpInst>(I)) {
            UncheckedOp = Cmp->getOperand(0);
        }
        else if (auto *RI = dyn_cast<ReturnInst>(I)) {
            if (RI->getReturnValue()) UncheckedOp = RI->getReturnValue();
        }
        else if (auto *CI = dyn_cast<CallInst>(I)) {
            if (CI->getNumOperands() > 0) {
                for (unsigned i = 0; i < CI->arg_size(); ++i) {
                    Value *Arg = CI->getArgOperand(i);
                    if (!isa<Function>(Arg)) {
                        Sinks[Arg] = new std::set<const Value *>();
                        Sinks[Arg]->insert(CI);
                    }
                }
                return;
            }
        }
        else if (auto *SI = dyn_cast<StoreInst>(I)) {
            UncheckedOp = SI->getValueOperand();
        }
        
        if (UncheckedOp) {
            Sinks[UncheckedOp] = new std::set<const Value *>();
            Sinks[UncheckedOp]->insert(I);
        }
    });
}

bool UseOfUninitializedVariableChecker::isValidTransfer(const Value * /*From*/, const Value *To) const {
    if (auto *CI = dyn_cast<CallInst>(To)) {
        if (auto *F = CI->getCalledFunction()) {
            if (isInitializationFunction(F->getName())) {
                return false; // Sanitized
            }
        }
    }
    return true;
}

int UseOfUninitializedVariableChecker::registerBugType() {
    BugReportMgr& mgr = BugReportMgr::get_instance();
    return mgr.register_bug_type(
        "Use of Uninitialized Variable",
        BugDescription::BI_HIGH,
        BugDescription::BC_SECURITY,
        "CWE-457"
    );
}

void UseOfUninitializedVariableChecker::reportVulnerability(
    int bugTypeId, const Value *Source, const Value *Sink, 
    const std::set<const Value *> *SinkInsts) {
    
    BugReport* report = new BugReport(bugTypeId);
    
    if (auto *SourceInst = dyn_cast<Instruction>(Source)) {
        std::string desc = "Uninitialized value originates here";
        if (isa<AllocaInst>(SourceInst)) {
            desc = "Local variable allocated without initialization";
        } else if (isa<LoadInst>(SourceInst)) {
            desc = "Load from uninitialized memory";
        }
        report->append_step(const_cast<Instruction*>(SourceInst), desc);
    }
    
    if (GVFA && Sink) {
        try {
            std::vector<const Value *> witnessPath = GVFA->getWitnessPath(Source, Sink);
            if (witnessPath.size() > 2) {
                for (size_t i = 1; i + 1 < witnessPath.size(); ++i) {
                    const Value *V = witnessPath[i];
                    if (!V || !isa<Instruction>(V)) continue;
                    const Instruction *I = cast<Instruction>(V);
                    report->append_step(const_cast<Instruction*>(I), 
                                       "Potentially uninitialized value propagates");
                }
            }
        } catch (...) {}
    }
    
    if (SinkInsts) {
        for (const Value *SI : *SinkInsts) {
            if (auto *SinkInst = dyn_cast<Instruction>(SI)) {
                std::string desc = "Use of potentially uninitialized value";
                if (isa<ReturnInst>(SinkInst)) {
                    desc = "Return of potentially uninitialized value";
                } else if (isa<CallInst>(SinkInst)) {
                    desc = "Potentially uninitialized value passed to function";
                } else if (isa<StoreInst>(SinkInst)) {
                    desc = "Store of potentially uninitialized value";
                }
                report->append_step(const_cast<Instruction*>(SinkInst), desc);
            }
        }
    }
    
    report->set_conf_score(75);
    BugReportMgr::get_instance().insert_report(bugTypeId, report);
}
