#include "Checker/GVFA/UseAfterFreeChecker.h"
#include "Checker/Report/BugReportMgr.h"
#include "Checker/Report/BugReport.h"
#include "Checker/Report/BugTypes.h"
#include "Analysis/GVFA/GlobalValueFlowAnalysis.h"
#include "Checker/GVFA/CheckerUtils.h"

#include <llvm/IR/Instructions.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;
using namespace CheckerUtils;

//===----------------------------------------------------------------------===//
// Source and Sink Identification
//===----------------------------------------------------------------------===//

void UseAfterFreeChecker::getSources(Module *M, VulnerabilitySourcesType &Sources) {
    forEachInstruction(M, [&Sources](const Instruction *I) {
        if (auto *Call = dyn_cast<CallInst>(I)) {
            if (isMemoryDeallocation(Call) && Call->arg_size() > 0) {
                // Mark the freed pointer (first argument) as a source
                auto *Arg = Call->getArgOperand(0);
                Sources[{Arg, 1}] = 1;
            }
        }
    });
}

void UseAfterFreeChecker::getSinks(Module *M, VulnerabilitySinksType &Sinks) {
    forEachInstruction(M, [&Sinks](const Instruction *I) {
        const Value *PtrOp = nullptr;
        
        if (auto *LI = dyn_cast<LoadInst>(I)) {
            PtrOp = LI->getPointerOperand();
        } else if (auto *SI = dyn_cast<StoreInst>(I)) {
            PtrOp = SI->getPointerOperand();
        } else if (auto *GEP = dyn_cast<GetElementPtrInst>(I)) {
            PtrOp = GEP->getPointerOperand();
        } else if (auto *Call = dyn_cast<CallInst>(I)) {
            if (auto *F = Call->getCalledFunction()) {
                StringRef Name = F->getName();
                if (doesLibFunctionDereferenceArg(Name, 0) || // Check basic dereferences first
                    Name.contains("memcpy") || Name.contains("memset") || 
                    Name.contains("strcpy") || Name.contains("strcmp")) {
                    
                    for (unsigned i = 0; i < Call->arg_size(); ++i) {
                        auto *Arg = Call->getArgOperand(i);
                        if (Arg->getType()->isPointerTy()) {
                            Sinks[Arg] = new std::set<const Value *>();
                            Sinks[Arg]->insert(Call);
                        }
                    }
                }
            }
        }
        
        if (PtrOp) {
            Sinks[PtrOp] = new std::set<const Value *>();
            Sinks[PtrOp]->insert(I);
        }
    });
}

bool UseAfterFreeChecker::isValidTransfer(const Value * /*From*/, const Value *To) const {
    if (auto *CI = dyn_cast<CallInst>(To)) {
        // Block flow through memory allocation functions - pointer becomes valid again
        if (isMemoryAllocation(CI)) {
            return false;
        }
    }
    return true;
}

int UseAfterFreeChecker::registerBugType() {
    BugReportMgr& mgr = BugReportMgr::get_instance();
    return mgr.register_bug_type(
        "Use After Free",
        BugDescription::BI_HIGH,
        BugDescription::BC_SECURITY,
        "CWE-416"
    );
}

void UseAfterFreeChecker::reportVulnerability(
    int bugTypeId, const Value *Source, const Value *Sink, 
    const std::set<const Value *> *SinkInsts) {
    
    BugReport* report = new BugReport(bugTypeId);
    
    if (auto *SourceInst = dyn_cast<Instruction>(Source)) {
        report->append_step(const_cast<Instruction*>(SourceInst), 
                           "Memory freed here");
    }
    
    if (GVFA && Sink) {
        try {
            std::vector<const Value *> witnessPath = GVFA->getWitnessPath(Source, Sink);
            if (witnessPath.size() > 2) {
                for (size_t i = 1; i + 1 < witnessPath.size(); ++i) {
                    const Value *V = witnessPath[i];
                    if (!V) continue;
                    const Instruction *I = dyn_cast<Instruction>(V);
                    if (!I) continue;
                    
                    std::string desc;
                    if (isa<StoreInst>(I)) {
                        desc = "Freed pointer stored to memory";
                    } else if (isa<LoadInst>(I)) {
                        desc = "Freed pointer loaded from memory";
                    } else if (isa<CallInst>(I)) {
                        desc = "Freed pointer passed in function call";
                    } else if (isa<ReturnInst>(I)) {
                        desc = "Freed pointer returned";
                    } else if (isa<PHINode>(I)) {
                        desc = "Freed pointer from control flow merge";
                    } else if (isa<GetElementPtrInst>(I)) {
                        desc = "Pointer arithmetic on freed pointer";
                    } else {
                        desc = "Freed pointer propagates through here";
                    }
                    report->append_step(const_cast<Instruction*>(I), desc);
                }
            }
        } catch (...) {}
    }
    
    if (SinkInsts) {
        for (const Value *SI : *SinkInsts) {
            if (auto *SinkInst = dyn_cast<Instruction>(SI)) {
                std::string sinkDesc = "Use of freed memory";
                if (isa<LoadInst>(SinkInst)) {
                    sinkDesc = "Load from freed memory";
                } else if (isa<StoreInst>(SinkInst)) {
                    sinkDesc = "Store to freed memory";
                } else if (isa<GetElementPtrInst>(SinkInst)) {
                    sinkDesc = "GEP on freed memory";
                } else if (isa<CallInst>(SinkInst)) {
                    sinkDesc = "Function call with freed memory";
                }
                report->append_step(const_cast<Instruction*>(SinkInst), sinkDesc);
            }
        }
    }
    
    report->set_conf_score(75);
    BugReportMgr::get_instance().insert_report(bugTypeId, report);
}
