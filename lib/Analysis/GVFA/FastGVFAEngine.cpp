#include "Analysis/GVFA/FastGVFAEngine.h"
#include "Analysis/GVFA/GVFAUtils.h"
#include "Utils/LLVM/RecursiveTimer.h"
#include <llvm/Support/Debug.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;
using namespace gvfa;

#define DEBUG_TYPE "dyck-gvfa"

void FastGVFAEngine::run() {
    RecursiveTimer Timer("DyckGVFA-Optimized");
    
    ReachabilityMap.clear();
    while (!SourcesVec.empty()) {
        LLVM_DEBUG(dbgs() << "[DEBUG] SrcVecSzBeforeExtend: " << SourcesVec.size() << "\n");
        extendSources(SourcesVec);
        LLVM_DEBUG(dbgs() << "[DEBUG] SrcVecSzAfterExtend: " << SourcesVec.size() << "\n");
        forwardRun();
    }
    
    LLVM_DEBUG({
        unsigned I = 0; 
        for (auto &It : ReachabilityMap) {
            if (It.second) ++I;
        } 
        dbgs() << "[DEBUG] ReachableNodesSz: " << I << "\n";
    });
    
    outs() << "[Opt-Indexing FW] Map Size: " << ReachabilityMap.size() << "\n";
    
    BackwardReachabilityMap.clear();
    backwardRun();
    
    outs() << "[Opt-Indexing BW] Map Size: " << BackwardReachabilityMap.size() << "\n";
}

bool FastGVFAEngine::srcReachable(const Value *V, const Value *Src) const {
    // Fast mode does not track specific source reachability
    // It only knows if V is reachable from SOME source in a Mask
    // Returning false is safer or we could throw? Original code returned false via empty check.
    return false;
}

std::vector<const Value *> FastGVFAEngine::getWitnessPath(const Value *From, const Value *To) const {
    // Use unguided BFS
    return GVFAUtils::getWitnessPath(From, To, VFG);
}

void FastGVFAEngine::forwardRun() {
    // Move sources to process list to clear SourcesVec for next iteration if needed?
    // Actually extendSources modifies SourcesVec.
    // optimizedForwardRun iterated over Sources.
    
    // We iterate over a copy or just the vector.
    // In original code: auto SourceList = std::move(Sources);
    // This implies SourcesVec is CLEARED after this?
    // Yes, because the outer loop checks !SourcesVec.empty().
    
    auto SourceList = std::move(SourcesVec);
    // SourcesVec is now empty.
    
    for (const auto &Src : SourceList) {
        forwardReachability(Src.first, Src.second);
    }
}

void FastGVFAEngine::backwardRun() {
    for (const auto &Sink : Sinks) {
        backwardReachability(Sink.first);
    }
}

void FastGVFAEngine::forwardReachability(const Value *V, int Mask) {
    std::queue<std::pair<const Value *, int>> WorkQueue;
    std::unordered_set<const Value *> Visited;
    
    WorkQueue.emplace(V, Mask);
    
    while (!WorkQueue.empty()) {
        auto front = WorkQueue.front();
        const Value *CurrentValue = front.first;
        int CurrentMask = front.second;
        WorkQueue.pop();
        
        if (!Visited.insert(CurrentValue).second) continue;
        
        ReachabilityMap[CurrentValue] |= CurrentMask;
        
        if (auto *CI = dyn_cast<CallInst>(CurrentValue)) {
            processCallSite(CI, CurrentValue, CurrentMask, WorkQueue);
        } else if (auto *RI = dyn_cast<ReturnInst>(CurrentValue)) {
            processReturnSite(RI, CurrentValue, CurrentMask, WorkQueue);
        }
        
        if (auto *Node = VFG->getVFGNode(const_cast<Value *>(CurrentValue))) {
            for (auto It = Node->begin(); It != Node->end(); ++It) {
                auto *Succ = It->first->getValue();
                if (int UncoveredMask = count(Succ, CurrentMask)) {
                    if (Visited.find(Succ) == Visited.end()) {
                        WorkQueue.emplace(Succ, UncoveredMask);
                    }
                }
            }
        }
    }
}

void FastGVFAEngine::backwardReachability(const Value *V) {
    std::queue<const Value *> WorkQueue;
    std::unordered_set<const Value *> Visited;
    
    WorkQueue.push(V);
    
    while (!WorkQueue.empty()) {
        const Value *CurrentValue = WorkQueue.front();
        WorkQueue.pop();
        
        if (!Visited.insert(CurrentValue).second) continue;
        
        BackwardReachabilityMap[CurrentValue] += 1;
        
        if (auto *Node = VFG->getVFGNode(const_cast<Value *>(CurrentValue))) {
            for (auto It = Node->in_begin(); It != Node->in_end(); ++It) {
                auto *Pred = It->first->getValue();
                if (!backwardCount(Pred) && Visited.find(Pred) == Visited.end()) {
                    WorkQueue.push(Pred);
                }
            }
        }
    }
}

