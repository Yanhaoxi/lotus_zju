#ifndef ANALYSIS_GVFA_GLOBALVALUEFLOWANALYSIS_H
#define ANALYSIS_GVFA_GLOBALVALUEFLOWANALYSIS_H

#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <queue>

#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>
#include <llvm/Support/raw_ostream.h>

#include "Alias/DyckAA/DyckVFG.h"
#include "Alias/DyckAA/DyckAliasAnalysis.h"
#include "Alias/DyckAA/DyckModRefAnalysis.h"
#include "Analysis/GVFA/GVFAUtils.h"

using namespace llvm;

// Forward declaration to avoid circular dependency
class GVFAVulnerabilityChecker;

namespace gvfa {
    class GVFAEngine;
}

// Vulnerability source/sink types
using ValueSitePairType = std::pair<const Value *, int>;
using VulnerabilitySourcesType = std::map<ValueSitePairType, int>;
using VulnerabilitySinksType = std::map<const Value *, std::set<const Value *> *>;

/**
 * Global Value Flow Analysis using Dyck VFG
 * 
 * Tracks data flow from vulnerability sources to sinks.
 */
class DyckGlobalValueFlowAnalysis {
public:
    long AllQueryCounter = 0;
    long SuccsQueryCounter = 0;
    long SnapshotedOnlineTime = 0;

private:
    // Core components
    DyckVFG *VFG = nullptr;
    DyckAliasAnalysis *DyckAA = nullptr;
    DyckModRefAnalysis *DyckMRA = nullptr;
    Module *M = nullptr;
    
    // Sources and sinks
    VulnerabilitySourcesType Sources;
    std::vector<std::pair<const Value *, int>> SourcesVec;
    VulnerabilitySinksType Sinks;
    
    // Vulnerability checker
    std::unique_ptr<GVFAVulnerabilityChecker> VulnChecker;

    // Analysis Engine
    std::unique_ptr<gvfa::GVFAEngine> Engine;

public:
    DyckGlobalValueFlowAnalysis(Module *M, DyckVFG *VFG, DyckAliasAnalysis *DyckAA, 
                               DyckModRefAnalysis *DyckMRA);
    
    ~DyckGlobalValueFlowAnalysis();
    
    // Public interface
    void setVulnerabilityChecker(std::unique_ptr<GVFAVulnerabilityChecker> checker);
    void run();
    
    // Query interfaces
    int reachable(const Value *V, int Mask);
    bool backwardReachable(const Value *V);
    bool srcReachable(const Value *V, const Value *Src) const;
    bool backwardReachableSink(const Value *V);
    bool backwardReachableAllSinks(const Value *V);
    
    // CFL reachability
    bool cflReachable(const Value *From, const Value *To) const;
    bool cflBackwardReachable(const Value *From, const Value *To) const;
    bool contextSensitiveReachable(const Value *From, const Value *To) const;
    bool contextSensitiveBackwardReachable(const Value *From, const Value *To) const;
    
    // Path extraction for bug reporting
    std::vector<const Value *> getWitnessPath(const Value *From, const Value *To) const;
    
    // Utilities
    void printOnlineQueryTime(llvm::raw_ostream &O, const char *Title = "[Online]") const;
    GVFAVulnerabilityChecker* getVulnerabilityChecker() const { return VulnChecker.get(); }

private:
    // Online reachability helpers
    bool onlineReachability(const Value *Target);
    bool onlineForwardReachability(const Value *Node, std::unordered_set<const Value *> &visited);
    bool onlineBackwardReachability(const Value *Node, const Value *Target, std::unordered_set<const Value *> &visited);
    
    // CFL helpers
    void initializeCFLAnalyzer();
    bool performCFLReachabilityQuery(const Value *From, const Value *To, bool Forward) const;
    bool cflReachabilityQuery(const Value *From, const Value *To, bool Forward) const;
    int getValueNodeID(const Value *V) const;
};

#endif // ANALYSIS_GVFA_GLOBALVALUEFLOWANALYSIS_H
