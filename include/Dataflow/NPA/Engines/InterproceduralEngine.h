#ifndef NPA_INTERPROCEDURAL_ENGINE_H
#define NPA_INTERPROCEDURAL_ENGINE_H

#include "Dataflow/NPA/NPA.h"
#include <llvm/IR/CFG.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <deque>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace npa {

// k-CFA context type
using CallSiteID = const llvm::Instruction*;
using CallString = std::vector<CallSiteID>;

// Helper to stringify call string
[[maybe_unused]] static inline std::string getCallStringSuffix(const CallString& cs) {
    if (cs.empty()) return "";
    std::ostringstream oss;
    oss << "@CS";
    for (auto *site : cs) {
        oss << ":" << (const void*)site;
    }
    return oss.str();
}

template <class D, class Analysis, int K = 0>
class InterproceduralEngine {
public:
    using Exp = Exp0<D>;
    using E = E0<D>;
    using Val = typename D::value_type;  // Summary type (Phase 1)
    using Fact = typename Analysis::FactType;  // Fact type (Phase 2)

    struct Result {
        // Summary at Function Exit (Phase 1)
        // Keyed by Function + Context
        std::map<std::string, Val> summaries;
        
        // Fact at Basic Block Entry (Phase 2)
        std::map<std::string, Fact> blockEntryFacts;
    };

    static std::string getBlockSymbol(const llvm::BasicBlock *BB, const CallString& cs) {
        std::ostringstream oss;
        oss << (const void*)BB << getCallStringSuffix(cs);
        return oss.str();
    }

    static std::string getFuncSymbol(const llvm::Function *F, const CallString& cs) {
        if (F->hasName()) return F->getName().str() + getCallStringSuffix(cs);
        std::ostringstream oss;
        oss << "Func_" << (const void*)F << getCallStringSuffix(cs);
        return oss.str();
    }

    // Helper to append call site to context
    static CallString pushContext(const CallString& cs, const llvm::Instruction* site) {
        CallString next = cs;
        next.push_back(site);
        if (next.size() > K) next.erase(next.begin()); // Keep last K
        return next;
    }

private:
    template <typename A>
    static auto getCallEntryTransfer(A &analysis,
                                     const llvm::CallInst &call,
                                     const llvm::Function &callee,
                                     int) -> decltype(analysis.getCallEntryTransfer(call, callee)) {
        return analysis.getCallEntryTransfer(call, callee);
    }

    static typename D::value_type getCallEntryTransfer(Analysis &,
                                                       const llvm::CallInst &,
                                                       const llvm::Function &,
                                                       long) {
        return D::one();
    }

    template <typename A>
    static auto getCallReturnTransfer(A &analysis,
                                      const llvm::CallInst &call,
                                      const llvm::Function &callee,
                                      int) -> decltype(analysis.getCallReturnTransfer(call, callee)) {
        return analysis.getCallReturnTransfer(call, callee);
    }

    static typename D::value_type getCallReturnTransfer(Analysis &,
                                                        const llvm::CallInst &,
                                                        const llvm::Function &,
                                                        long) {
        return D::one();
    }

    template <typename A>
    static auto getCallToReturnTransfer(A &analysis,
                                        const llvm::CallInst &call,
                                        int) -> decltype(analysis.getCallToReturnTransfer(call)) {
        return analysis.getCallToReturnTransfer(call);
    }

    static typename D::value_type getCallToReturnTransfer(Analysis &,
                                                          const llvm::CallInst &,
                                                          long) {
        return D::one();
    }

public:
    static Result run(llvm::Module &M, Analysis &analysis, bool verbose = false) {
        std::vector<std::pair<Symbol, E>> eqns;
        
        // 1. Phase 1: Build Equations & Summaries (Worklist driven for Reachable Contexts)
        
        // Worklist of (Function, Context)
        std::deque<std::pair<llvm::Function*, CallString>> worklist;
        std::set<std::pair<llvm::Function*, CallString>> visited;
        
        llvm::Function *Main = M.getFunction("main");
        if (Main) {
            worklist.push_back({Main, {}});
            visited.insert({Main, {}});
        } else {
             for (auto &F : M) {
                if (!F.isDeclaration()) {
                     worklist.push_back({&F, {}});
                     visited.insert({&F, {}});
                }
            }
        }

        while(!worklist.empty()) {
            auto item = worklist.front();
            worklist.pop_front();
            llvm::Function *F = item.first;
            CallString cs = item.second;
            
            std::string fSym = getFuncSymbol(F, cs);
            E exitExpr = nullptr;

            for (auto &BB : *F) {
                std::string bSym = getBlockSymbol(&BB, cs);

                // Entry to Block (Joins from Predecessors)
                E inExpr = nullptr;
                if (&BB == &F->getEntryBlock()) {
                    inExpr = Exp::term(D::one());
                } else {
                    bool hasPreds = false;
                    for (auto *Pred : predecessors(&BB)) {
                        hasPreds = true;
                        std::string predSym = getBlockSymbol(Pred, cs); // Intra-procedural: same context
                        auto pHole = Exp::hole(predSym);
                        if (!inExpr) inExpr = pHole;
                        else inExpr = Exp::ndet(inExpr, pHole);
                    }
                    if (!hasPreds) inExpr = Exp::term(D::zero());
                }

                // Block Body (Instruction Transfer Functions)
                E currentPath = inExpr;
                for (auto &I : BB) {
                    if (auto *CI = llvm::dyn_cast<llvm::CallInst>(&I)) {
                        if (auto *Callee = CI->getCalledFunction()) {
                            if (!Callee->isDeclaration()) {
                                CallString calleeCS = pushContext(cs, CI);
                                if (visited.find({Callee, calleeCS}) == visited.end()) {
                                    visited.insert({Callee, calleeCS});
                                    worklist.push_back({Callee, calleeCS});
                                }
                                currentPath = Exp::seq(getCallEntryTransfer(analysis, *CI, *Callee, 0), currentPath);
                                currentPath = Exp::call(getFuncSymbol(Callee, calleeCS), currentPath);
                                currentPath = Exp::seq(getCallReturnTransfer(analysis, *CI, *Callee, 0), currentPath);
                            } else {
                                currentPath = Exp::seq(getCallToReturnTransfer(analysis, *CI, 0), currentPath);
                            }
                        } else {
                            currentPath = Exp::seq(getCallToReturnTransfer(analysis, *CI, 0), currentPath);
                        }
                    }
                    currentPath = analysis.getTransfer(I, currentPath);
                }

                eqns.emplace_back(bSym, currentPath);

                if (llvm::succ_begin(&BB) == llvm::succ_end(&BB)) {
                    if (!exitExpr) exitExpr = Exp::hole(bSym);
                    else exitExpr = Exp::ndet(exitExpr, Exp::hole(bSym));
                }
            }

            if (!exitExpr) exitExpr = Exp::term(D::zero());
            eqns.emplace_back(fSym, exitExpr);
        }

        // Solve Summaries (Newtonian)
        auto rawRes = NewtonSolver<D>::solve(eqns, verbose);
        
        // Convert result to map for random access
        // Note: NPA I0::eval uses std::map, so we use that here
        std::map<Symbol, Val> solvedMap;
        for (auto &p : rawRes.first) solvedMap[p.first] = p.second;

        // 2. Phase 2: Top-Down Propagation
        Result res;
        res.summaries = solvedMap; // Store all computed summaries

        // Worklist for Phase 2 (Function, Context)
        std::deque<std::pair<llvm::Function*, CallString>> worklist2;
        std::set<std::pair<llvm::Function*, CallString>> inWorklist2;
        std::unordered_map<std::string, Fact> funcInput; 

        if (Main) {
            std::string sym = getFuncSymbol(Main, {});
            funcInput[sym] = analysis.getEntryValue();
            worklist2.push_back({Main, {}});
            inWorklist2.insert({Main, {}});
        } else {
             for (auto &F : M) {
                if (!F.isDeclaration()) {
                     std::string sym = getFuncSymbol(&F, {});
                     funcInput[sym] = analysis.getEntryValue();
                     worklist2.push_back({&F, {}});
                     inWorklist2.insert({&F, {}});
                }
            }
        }

        while (!worklist2.empty()) {
            auto item = worklist2.front();
            worklist2.pop_front();
            llvm::Function *F = item.first;
            CallString cs = item.second;
            inWorklist2.erase(item);

            std::string fSym = getFuncSymbol(F, cs);
            Fact inputVal = funcInput[fSym];

            for (auto &BB : *F) {
                std::string bSym = getBlockSymbol(&BB, cs);
                if (!solvedMap.count(bSym)) continue;

                // Compute Facts at Block Entry
                Val entryToBlockStart = D::zero();
                if (&BB == &F->getEntryBlock()) {
                    entryToBlockStart = D::one();
                } else {
                    bool first = true;
                    for (auto *Pred : predecessors(&BB)) {
                        std::string pSym = getBlockSymbol(Pred, cs);
                        if (solvedMap.count(pSym)) {
                            if (first) { entryToBlockStart = solvedMap[pSym]; first=false; }
                            else { entryToBlockStart = D::combine(entryToBlockStart, solvedMap[pSym]); }
                        }
                    }
                }
                
                auto blockEntryFact = analysis.applySummary(entryToBlockStart, inputVal);
                res.blockEntryFacts[bSym] = blockEntryFact;

                // Process Calls to propagate to Callees
                E currentPath = Exp::term(D::one());
                
                for (auto &I : BB) {
                    if (auto *CI = llvm::dyn_cast<llvm::CallInst>(&I)) {
                        if (auto *Callee = CI->getCalledFunction()) {
                            if (!Callee->isDeclaration()) {
                                // 1. Propagate facts to callee
                                CallString calleeCS = pushContext(cs, CI);
                                std::string calleeFSym = getFuncSymbol(Callee, calleeCS);

                                // Eval summary path to call site
                                Val currentPathVal = I0<D>::eval(false, solvedMap, currentPath);
                                Val callEntry = D::extend(getCallEntryTransfer(analysis, *CI, *Callee, 0),
                                                          currentPathVal);
                                Val totalToCall = D::extend(callEntry, entryToBlockStart);

                                auto factAtCall = analysis.applySummary(totalToCall, inputVal);

                                // Update Callee Input
                                if (!funcInput.count(calleeFSym)) {
                                    funcInput[calleeFSym] = factAtCall;
                                    if (inWorklist2.find({Callee, calleeCS}) == inWorklist2.end()) {
                                        worklist2.push_back({Callee, calleeCS});
                                        inWorklist2.insert({Callee, calleeCS});
                                    }
                                } else {
                                    auto oldVal = funcInput[calleeFSym];
                                    auto newVal = analysis.joinFacts(oldVal, factAtCall);
                                    if (!analysis.factsEqual(oldVal, newVal)) {
                                        funcInput[calleeFSym] = newVal;
                                        if (inWorklist2.find({Callee, calleeCS}) == inWorklist2.end()) {
                                            worklist2.push_back({Callee, calleeCS});
                                            inWorklist2.insert({Callee, calleeCS});
                                        }
                                    }
                                }
                            }
                        }
                    }
                    
                    // 2. Update local path
                    if (auto *CI = llvm::dyn_cast<llvm::CallInst>(&I)) {
                         if (auto *Callee = CI->getCalledFunction()) {
                             if (!Callee->isDeclaration()) {
                                 CallString calleeCS = pushContext(cs, CI);
                                 currentPath = Exp::seq(getCallEntryTransfer(analysis, *CI, *Callee, 0), currentPath);
                                 currentPath = Exp::call(getFuncSymbol(Callee, calleeCS), currentPath);
                                 currentPath = Exp::seq(getCallReturnTransfer(analysis, *CI, *Callee, 0), currentPath);
                             } else {
                                 currentPath = Exp::seq(getCallToReturnTransfer(analysis, *CI, 0), currentPath);
                             }
                         }
                    }
                    currentPath = analysis.getTransfer(I, currentPath);
                }
            }
        }
        return res;
    }
};

} // namespace npa

#endif
