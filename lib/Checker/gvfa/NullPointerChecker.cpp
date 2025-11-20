#include "Checker/GVFA/NullPointerChecker.h"
#include "Checker/Report/BugReportMgr.h"
#include "Checker/Report/BugReport.h"
#include "Checker/Report/BugTypes.h"
#include "Analysis/GVFA/GlobalValueFlowAnalysis.h"
#include "Analysis/NullPointer/NullCheckAnalysis.h"
#include "Analysis/NullPointer/ContextSensitiveNullCheckAnalysis.h"
#include "Analysis/NullPointer/ContextSensitiveNullFlowAnalysis.h"
#include "Checker/GVFA/CheckerUtils.h"

#include <llvm/IR/Instructions.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;
using namespace CheckerUtils;

//===----------------------------------------------------------------------===//
// Source and Sink Identification
//===----------------------------------------------------------------------===//

void NullPointerChecker::getSources(Module *M, VulnerabilitySourcesType &Sources) {
    forEachInstruction(M, [&Sources](const Instruction *I) {
        // 1. NULL constants stored to variables
        if (auto *SI = dyn_cast<StoreInst>(I)) {
            if (isa<ConstantPointerNull>(SI->getValueOperand())) {
                Sources[{SI, 1}] = 1;
            }
        }
        // 2. Memory allocation functions (can return NULL on failure)
        else if (auto *Call = dyn_cast<CallInst>(I)) {
            if (isMemoryAllocation(Call)) {
                Sources[{Call, 1}] = 1;
            }
        }
    });
}

void NullPointerChecker::getSinks(Module *M, VulnerabilitySinksType &Sinks) {
    auto addSink = [&](const Value *PtrOp, const Instruction *I) {
        if (!PtrOp || !PtrOp->getType()->isPointerTy()) return;
        if (isProvenNonNull(PtrOp, I)) return; // Filter out proven safe pointers
        
        if (Sinks.find(PtrOp) == Sinks.end()) {
            Sinks[PtrOp] = new std::set<const Value *>();
        }
        Sinks[PtrOp]->insert(I);
    };
    
    forEachInstruction(M, [&](const Instruction *I) {
        // Direct dereferences
        if (auto *LI = dyn_cast<LoadInst>(I)) {
            addSink(LI->getPointerOperand(), I);
        } 
        else if (auto *SI = dyn_cast<StoreInst>(I)) {
            addSink(SI->getPointerOperand(), I);
        } 
        else if (auto *GEP = dyn_cast<GetElementPtrInst>(I)) {
            addSink(GEP->getPointerOperand(), I);
        }
        // Library function calls
        else if (auto *Call = dyn_cast<CallInst>(I)) {
            if (auto *F = Call->getCalledFunction()) {
                StringRef Name = F->getName();
                for (unsigned i = 0; i < Call->arg_size(); i++) {
                    if (doesLibFunctionDereferenceArg(Name, i)) {
                        addSink(Call->getArgOperand(i), I);
                    }
                }
            }
        }
    });
}

bool NullPointerChecker::isValidTransfer(const Value * /*From*/, const Value *To) const {
    if (auto *CI = dyn_cast<CallInst>(To)) {
        if (auto *F = CI->getCalledFunction()) {
            StringRef Name = F->getName();
            // Block flow through null check functions
            return !Name.contains("check") && !Name.contains("assert");
        }
    }
    return true;
}

bool NullPointerChecker::isProvenNonNull(const Value *Ptr, const Instruction *Inst) const {
    if (!NCA && !CSNCA) return false;
    
    auto *MutablePtr = const_cast<Value *>(Ptr);
    auto *MutableInst = const_cast<Instruction *>(Inst);
    
    if (NCA) return !NCA->mayNull(MutablePtr, MutableInst);
    if (CSNCA) {
        Context emptyCtx;
        return !CSNCA->mayNull(MutablePtr, MutableInst, emptyCtx);
    }
    return false;
}

//===----------------------------------------------------------------------===//
// Bug Reporting
//===----------------------------------------------------------------------===//

int NullPointerChecker::registerBugType() {
    BugReportMgr& mgr = BugReportMgr::get_instance();
    return mgr.register_bug_type(
        "NULL Pointer Dereference",
        BugDescription::BI_HIGH,
        BugDescription::BC_SECURITY,
        "CWE-476, CWE-690"
    );
}

void NullPointerChecker::reportVulnerability(
    int bugTypeId, const Value *Source, const Value *Sink, 
    const std::set<const Value *> *SinkInsts) {
    
    BugReport* report = new BugReport(bugTypeId);
    
    if (auto *SourceInst = dyn_cast<Instruction>(Source)) {
        report->append_step(const_cast<Instruction*>(SourceInst), 
                           "Null value originates here");
    } else {
        if (SinkInsts && !SinkInsts->empty()) {
            if (auto *FirstSinkInst = dyn_cast<Instruction>(*SinkInsts->begin())) {
                std::string sourceDesc = "Null value source: ";
                llvm::raw_string_ostream OS(sourceDesc);
                Source->print(OS);
                report->append_step(const_cast<Instruction*>(FirstSinkInst), OS.str());
            }
        }
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
                        desc = "Null value stored to memory";
                    } else if (isa<LoadInst>(I)) {
                        desc = "Potentially null value loaded from memory";
                    } else if (isa<CallInst>(I)) {
                        desc = "Potentially null value passed in function call";
                    } else if (isa<ReturnInst>(I)) {
                        desc = "Potentially null value returned";
                    } else if (isa<PHINode>(I)) {
                        desc = "Potentially null value from control flow merge";
                    } else if (isa<GetElementPtrInst>(I)) {
                        desc = "Pointer arithmetic on potentially null value";
                    } else {
                        desc = "Value propagates through here";
                    }
                    report->append_step(const_cast<Instruction*>(I), desc);
                }
            }
        } catch (...) {}
    }
    
    if (SinkInsts) {
        for (const Value *SI : *SinkInsts) {
            if (auto *SinkInst = dyn_cast<Instruction>(SI)) {
                std::string sinkDesc = "Potential null pointer dereference";
                if (isa<LoadInst>(SinkInst)) {
                    sinkDesc = "Load from potentially null pointer";
                } else if (isa<StoreInst>(SinkInst)) {
                    sinkDesc = "Store to potentially null pointer";
                } else if (isa<GetElementPtrInst>(SinkInst)) {
                    sinkDesc = "GEP on potentially null pointer";
                }
                report->append_step(const_cast<Instruction*>(SinkInst), sinkDesc);
            }
        }
    }
    
    int confidence = (NCA || CSNCA) ? 85 : 70;
    report->set_conf_score(confidence);
    BugReportMgr::get_instance().insert_report(bugTypeId, report);
}
