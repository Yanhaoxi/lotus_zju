#include "Analysis/GVFA/PreciseGVFAEngine.h"
#include "Analysis/GVFA/GVFAUtils.h"
#include "Utils/LLVM/RecursiveTimer.h"
#include <llvm/Support/Debug.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;
using namespace gvfa;

#define DEBUG_TYPE "dyck-gvfa"

void PreciseGVFAEngine::run() {
    RecursiveTimer Timer("DyckGVFA-Detailed");
    
    AllReachabilityMap.clear();
    while (!SourcesVec.empty()) {
        LLVM_DEBUG(dbgs() << "[DEBUG] SrcVecSzBeforeExtend: " << SourcesVec.size() << "\n");
        extendSources(SourcesVec);
        LLVM_DEBUG(dbgs() << "[DEBUG] SrcVecSzAfterExtend: " << SourcesVec.size() << "\n");
        detailedForwardRun();
    }
    
    outs() << "[Indexing FW] Map Size: " << AllReachabilityMap.size() << "\n";
    
    AllBackwardReachabilityMap.clear();
    detailedBackwardRun();
    
    outs() << "[Indexing BW] Map Size: " << AllBackwardReachabilityMap.size() << "\n";
}

int PreciseGVFAEngine::reachable(const Value *V, int Mask) {
    int ResultMask = 0;
    auto It = AllReachabilityMap.find(V);
    if (It != AllReachabilityMap.end()) {
        for (const Value *Src : It->second) {
            auto SrcIt = ReachabilityMap.find(Src);
            if (SrcIt != ReachabilityMap.end()) {
                ResultMask |= (SrcIt->second & Mask);
            }
        }
    }
    return ResultMask;
}

bool PreciseGVFAEngine::srcReachable(const Value *V, const Value *Src) const {
    auto It = AllReachabilityMap.find(V);
    return (It != AllReachabilityMap.end()) && It->second.count(Src);
}

bool PreciseGVFAEngine::backwardReachableSink(const Value *V) {
    auto It = AllBackwardReachabilityMap.find(V);
    if (It != AllBackwardReachabilityMap.end() && !It->second.empty()) {
        return true;
    }
    return false;
}

bool PreciseGVFAEngine::backwardReachableAllSinks(const Value *V) {
    auto It = AllBackwardReachabilityMap.find(V);
    if (It == AllBackwardReachabilityMap.end()) {
        return false;
    }
    if (It->second.size() != Sinks.size()) {
        return false;
    }
    return true;
}

std::vector<const Value *> PreciseGVFAEngine::getWitnessPath(const Value *From, const Value *To) const {
    if (!AllReachabilityMap.empty()) {
        auto It = AllReachabilityMap.find(To);
        if (It == AllReachabilityMap.end() || !It->second.count(From)) {
            return {}; 
        }
        return GVFAUtils::getWitnessPathGuided(From, To, VFG, AllReachabilityMap);
    }
    return GVFAUtils::getWitnessPath(From, To, VFG);
}

void PreciseGVFAEngine::detailedForwardRun() {
    auto SourceList = std::move(SourcesVec);
    for (const auto &Src : SourceList) {
        detailedForwardReachability(Src.first, Src.first);
    }
}

void PreciseGVFAEngine::detailedBackwardRun() {
    for (const auto &Sink : Sinks) {
        detailedBackwardReachability(Sink.first, Sink.first);
    }
}

void PreciseGVFAEngine::detailedForwardReachability(const Value *V, const Value *Src) {
    std::queue<const Value *> WorkQueue;
    std::unordered_set<const Value *> Visited;
    
    WorkQueue.push(V);
    
    while (!WorkQueue.empty()) {
        const Value *CurrentValue = WorkQueue.front();
        WorkQueue.pop();
        
        if (!Visited.insert(CurrentValue).second) continue;
        
        AllReachabilityMap[CurrentValue].insert(Src);
        
        if (auto *Node = VFG->getVFGNode(const_cast<Value *>(CurrentValue))) {
            for (auto It = Node->begin(); It != Node->end(); ++It) {
                auto *Succ = It->first->getValue();
                if (!allCount(Succ, Src) && Visited.find(Succ) == Visited.end()) {
                    WorkQueue.push(Succ);
                }
            }
        }
    }
}

void PreciseGVFAEngine::detailedBackwardReachability(const Value *V, const Value *Sink) {
    std::queue<const Value *> WorkQueue;
    std::unordered_set<const Value *> Visited;
    
    WorkQueue.push(V);
    
    while (!WorkQueue.empty()) {
        const Value *CurrentValue = WorkQueue.front();
        WorkQueue.pop();
        
        if (!Visited.insert(CurrentValue).second) continue;
        
        AllBackwardReachabilityMap[CurrentValue].insert(Sink);
        
        if (auto *Node = VFG->getVFGNode(const_cast<Value *>(CurrentValue))) {
            for (auto It = Node->in_begin(); It != Node->in_end(); ++It) {
                auto *Pred = It->first->getValue();
                if (!allBackwardCount(Pred, Sink) && Visited.find(Pred) == Visited.end()) {
                    WorkQueue.push(Pred);
                }
            }
        }
    }
}

bool PreciseGVFAEngine::allCount(const Value *V, const Value *Src) {
    auto &Set = AllReachabilityMap[V];
    if (Set.count(Src)) {
        return true;
    } else {
        Set.insert(Src);
        return false;
    }
}

bool PreciseGVFAEngine::allBackwardCount(const Value *V, const Value *Sink) {
    auto &Set = AllBackwardReachabilityMap[V];
    if (Set.count(Sink)) {
        return true;
    } else {
        Set.insert(Sink);
        return false;
    }
}

