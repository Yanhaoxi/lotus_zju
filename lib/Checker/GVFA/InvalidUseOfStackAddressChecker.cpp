/*
 *
 * Author: rainoftime
*/
#include "Checker/GVFA/InvalidUseOfStackAddressChecker.h"
#include "Checker/Report/BugReportMgr.h"
#include "Checker/Report/BugReport.h"
#include "Checker/Report/BugTypes.h"
#include "Analysis/GVFA/GlobalValueFlowAnalysis.h"
#include "Checker/GVFA/CheckerUtils.h"

#include <llvm/IR/Instructions.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;
using namespace CheckerUtils;

//===----------------------------------------------------------------------===//
// Source and Sink Identification
//===----------------------------------------------------------------------===//

void InvalidUseOfStackAddressChecker::getSources(Module *M, VulnerabilitySourcesType &Sources) {
    forEachInstruction(M, [&Sources](const Instruction *I) {
        // Skip main function - stack addresses in main often have global lifetime
        if (I->getFunction()->getName() == "main") return;

        if (auto *AI = dyn_cast<AllocaInst>(I)) {
            Sources[{AI, 1}] = 1;
        }
    });
}

void InvalidUseOfStackAddressChecker::getSinks(Module *M, VulnerabilitySinksType &Sinks) {
    forEachInstruction(M, [&Sinks](const Instruction *I) {
        // Return instructions - stack address returned to caller
        if (auto *RI = dyn_cast<ReturnInst>(I)) {
            if (const Value *RetVal = RI->getReturnValue()) {
                if (RetVal->getType()->isPointerTy()) {
                    Sinks[RetVal] = new std::set<const Value *>();
                    Sinks[RetVal]->insert(RI);
                }
            }
        }
        // Stores to global variables or escaped memory
        else if (auto *SI = dyn_cast<StoreInst>(I)) {
            const Value *PtrOp = SI->getPointerOperand();
            if (isa<GlobalVariable>(PtrOp)) {
                const Value *ValOp = SI->getValueOperand();
                if (ValOp->getType()->isPointerTy()) {
                    Sinks[ValOp] = new std::set<const Value *>();
                    Sinks[ValOp]->insert(SI);
                }
            }
        }
        // Function arguments that might store the pointer
        else if (auto *CI = dyn_cast<CallInst>(I)) {
            if (auto *Callee = CI->getCalledFunction()) {
                StringRef Name = Callee->getName();
                if (isSafeStackCaptureFunction(Name)) {
                    return;
                }
                
                for (unsigned i = 0; i < CI->arg_size(); ++i) {
                    const Value *Arg = CI->getArgOperand(i);
                    if (Arg->getType()->isPointerTy()) {
                        // Conservative: assume pointer arguments might escape
                        // unless function is known to be safe
                        if (Callee->isDeclaration()) {
                            Sinks[Arg] = new std::set<const Value *>();
                            Sinks[Arg]->insert(CI);
                        }
                    }
                }
            }
        }
    });
}

bool InvalidUseOfStackAddressChecker::isValidTransfer(const Value * /*From*/, const Value * /*To*/) const {
    return true;
}

int InvalidUseOfStackAddressChecker::registerBugType() {
    BugReportMgr& mgr = BugReportMgr::get_instance();
    return mgr.register_bug_type(
        "Invalid Use of Stack Address",
        BugDescription::BI_HIGH,
        BugDescription::BC_SECURITY,
        "CWE-562"
    );
}

void InvalidUseOfStackAddressChecker::reportVulnerability(
    int bugTypeId, const Value *Source, const Value *Sink, 
    const std::set<const Value *> *SinkInsts) {
    
    BugReport* report = new BugReport(bugTypeId);
    
    if (auto *AI = dyn_cast<AllocaInst>(Source)) {
        report->append_step(const_cast<AllocaInst*>(AI), 
                          "Stack memory allocated here");
    } else if (auto *SourceInst = dyn_cast<Instruction>(Source)) {
        report->append_step(const_cast<Instruction*>(SourceInst), 
                          "Stack address originates here");
    }
    
    if (GVFA && Sink) {
        try {
            std::vector<const Value *> witnessPath = GVFA->getWitnessPath(Source, Sink);
            if (witnessPath.size() > 2) {
                for (size_t i = 1; i + 1 < witnessPath.size(); ++i) {
                    const Value *V = witnessPath[i];
                    if (!V || !isa<Instruction>(V)) continue;
                    const Instruction *I = cast<Instruction>(V);
                    std::string desc = "Stack address propagates";
                    if (isa<GetElementPtrInst>(I)) {
                        desc = "Pointer arithmetic on stack address";
                    } else if (isa<LoadInst>(I)) {
                        desc = "Stack address loaded from memory";
                    } else if (isa<StoreInst>(I)) {
                        desc = "Stack address stored to memory";
                    }
                    report->append_step(const_cast<Instruction*>(I), desc);
                }
            }
        } catch (...) {}
    }
    
    if (SinkInsts) {
        for (const Value *SI : *SinkInsts) {
            if (auto *RI = dyn_cast<ReturnInst>(SI)) {
                report->append_step(const_cast<ReturnInst*>(RI), 
                                  "Stack address returned (escapes scope)");
            } else if (auto *StoreI = dyn_cast<StoreInst>(SI)) {
                report->append_step(const_cast<StoreInst*>(StoreI), 
                                  "Stack address stored to global memory");
            } else if (auto *CI = dyn_cast<CallInst>(SI)) {
                report->append_step(const_cast<CallInst*>(CI), 
                                  "Stack address passed to external function (may escape)");
            } else if (auto *SinkInst = dyn_cast<Instruction>(SI)) {
                report->append_step(const_cast<Instruction*>(SinkInst), 
                                  "Stack address escapes here");
            }
        }
    }
    
    report->set_conf_score(85);
    BugReportMgr::get_instance().insert_report(bugTypeId, report);
}
