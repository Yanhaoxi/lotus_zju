#include "Checker/GVFA/FreeOfNonHeapMemoryChecker.h"
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

void FreeOfNonHeapMemoryChecker::getSources(Module *M, VulnerabilitySourcesType &Sources) {
    forEachInstruction(M, [&Sources](const Instruction *I) {
        if (auto *AI = dyn_cast<AllocaInst>(I)) {
            Sources[{AI, 1}] = 1;
        }
    });
    
    for (auto &GV : M->globals()) {
        Sources[{&GV, 1}] = 1;
    }
}

void FreeOfNonHeapMemoryChecker::getSinks(Module *M, VulnerabilitySinksType &Sinks) {
    forEachInstruction(M, [&Sinks](const Instruction *I) {
        if (auto *CI = dyn_cast<CallInst>(I)) {
            if (isMemoryDeallocation(CI) && CI->arg_size() > 0) {
                const Value *PtrArg = CI->getArgOperand(0);
                Sinks[PtrArg] = new std::set<const Value *>();
                Sinks[PtrArg]->insert(CI);
            }
        }
    });
}

bool FreeOfNonHeapMemoryChecker::isValidTransfer(const Value * /*From*/, const Value *To) const {
    if (auto *CI = dyn_cast<CallInst>(To)) {
        if (isMemoryAllocation(CI)) {
            return false; // Block flow through allocation functions
        }
    }
    return true;
}

int FreeOfNonHeapMemoryChecker::registerBugType() {
    BugReportMgr& mgr = BugReportMgr::get_instance();
    return mgr.register_bug_type(
        "Free of Memory Not on the Heap",
        BugDescription::BI_HIGH,
        BugDescription::BC_SECURITY,
        "CWE-590"
    );
}

void FreeOfNonHeapMemoryChecker::reportVulnerability(
    int bugTypeId, const Value *Source, const Value *Sink, 
    const std::set<const Value *> *SinkInsts) {
    
    BugReport* report = new BugReport(bugTypeId);
    
    if (auto *AI = dyn_cast<AllocaInst>(Source)) {
        report->append_step(const_cast<AllocaInst*>(AI), 
                          "Stack memory allocated here");
    } else if (auto *GV = dyn_cast<GlobalVariable>(Source)) {
        if (SinkInsts && !SinkInsts->empty()) {
            if (auto *FirstSink = dyn_cast<Instruction>(*SinkInsts->begin())) {
                std::string desc = "Global variable '";
                desc += GV->getName().str();
                desc += "' is not on the heap";
                report->append_step(const_cast<Instruction*>(FirstSink), desc);
            }
        }
    } else if (auto *SourceInst = dyn_cast<Instruction>(Source)) {
        report->append_step(const_cast<Instruction*>(SourceInst), 
                          "Non-heap memory originates here");
    }
    
    if (GVFA && Sink) {
        try {
            std::vector<const Value *> witnessPath = GVFA->getWitnessPath(Source, Sink);
            if (witnessPath.size() > 2) {
                for (size_t i = 1; i + 1 < witnessPath.size(); ++i) {
                    const Value *V = witnessPath[i];
                    if (!V || !isa<Instruction>(V)) continue;
                    const Instruction *I = cast<Instruction>(V);
                    std::string desc = "Non-heap pointer propagates";
                    if (isa<GetElementPtrInst>(I)) {
                        desc = "Pointer arithmetic on non-heap memory";
                    } else if (isa<LoadInst>(I)) {
                        desc = "Non-heap pointer loaded from memory";
                    }
                    report->append_step(const_cast<Instruction*>(I), desc);
                }
            }
        } catch (...) {}
    }
    
    if (SinkInsts) {
        for (const Value *SI : *SinkInsts) {
            if (auto *CI = dyn_cast<CallInst>(SI)) {
                report->append_step(const_cast<CallInst*>(CI), 
                                  "Attempt to free non-heap memory");
            }
        }
    }
    
    report->set_conf_score(90);
    BugReportMgr::get_instance().insert_report(bugTypeId, report);
}
