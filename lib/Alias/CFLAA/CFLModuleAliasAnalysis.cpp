//===- CFLModuleAliasAnalysis.cpp - Module-scope CFL AA -------*- C++ -*-===//
// This file is newly-added by rainoftime.
// Its goal is to implement a module-level CFL AA that can be used to answer
// alias queries for the whole module.
//===----------------------------------------------------------------------===//

#include "Alias/CFLAA/CFLModuleAliasAnalysis.h"

#include "Alias/CFLAA/AliasAnalysisSummary.h"
#include "Alias/CFLAA/CFLAliasAnalysisUtils.h"
#include "Alias/CFLAA/CFLGraph.h"
#include "Alias/CFLAA/StratifiedSets.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"
#include <bitset>
#include <vector>

using namespace llvm;
using namespace llvm::cflaa;

#define DEBUG_TYPE "cfl-module-aa"

namespace {

/// Minimal analysis stub so CFLGraphBuilder can be reused without summaries.
class EmptySummaryProvider {
public:
  const AliasSummary *getAliasSummary(Function &) { return &Empty; }

private:
  AliasSummary Empty;
};

// Types needed for Anders-style analysis (copied from CFLAndersAliasAnalysis.cpp)
using StateSet = std::bitset<7>;
enum class MatchState : unsigned {
  FlowFromReadOnly = 0,
  FlowFromMemAliasNoReadWrite = 1,
  FlowFromMemAliasReadOnly = 2,
  FlowToWriteOnly = 3,
  FlowToReadWrite = 4,
  FlowToMemAliasWriteOnly = 5,
  FlowToMemAliasReadWrite = 6
};

// We use ReachabilitySet to keep track of value aliases (The nonterminal "V" in
// the paper) during the analysis.
class ReachabilitySet {
  using ValueStateMap = DenseMap<InstantiatedValue, StateSet>;
  using ValueReachMap = DenseMap<InstantiatedValue, ValueStateMap>;

  ValueReachMap ReachMap;

public:
  using const_valuestate_iterator = ValueStateMap::const_iterator;
  using const_value_iterator = ValueReachMap::const_iterator;

  // Insert edge 'From->To' at state 'State'
  bool insert(InstantiatedValue From, InstantiatedValue To, MatchState State) {
    assert(From != To);
    auto &States = ReachMap[To][From];
    auto Idx = static_cast<size_t>(State);
    if (!States.test(Idx)) {
      States.set(Idx);
      return true;
    }
    return false;
  }

  // Return the set of all ('From', 'State') pair for a given node 'To'
  iterator_range<const_valuestate_iterator>
  reachableValueAliases(InstantiatedValue V) const {
    auto Itr = ReachMap.find(V);
    if (Itr == ReachMap.end())
      return make_range<const_valuestate_iterator>(const_valuestate_iterator(),
                                                   const_valuestate_iterator());
    return make_range<const_valuestate_iterator>(Itr->second.begin(),
                                                 Itr->second.end());
  }

  iterator_range<const_value_iterator> value_mappings() const {
    return make_range<const_value_iterator>(ReachMap.begin(), ReachMap.end());
  }
};

// We use AliasMemSet to keep track of all memory aliases (the nonterminal "M"
// in the paper) during the analysis.
class AliasMemSet {
  using MemSet = DenseSet<InstantiatedValue>;
  using MemMapType = DenseMap<InstantiatedValue, MemSet>;

  MemMapType MemMap;

public:
  using const_mem_iterator = MemSet::const_iterator;

  bool insert(InstantiatedValue LHS, InstantiatedValue RHS) {
    // Top-level values can never be memory aliases because one cannot take the
    // addresses of them
    assert(LHS.DerefLevel > 0 && RHS.DerefLevel > 0);
    return MemMap[LHS].insert(RHS).second;
  }

  const MemSet *getMemoryAliases(InstantiatedValue V) const {
    auto Itr = MemMap.find(V);
    if (Itr == MemMap.end())
      return nullptr;
    return &Itr->second;
  }
};

// We use AliasAttrMap to keep track of the AliasAttr of each node.
class AliasAttrMap {
  using MapType = DenseMap<InstantiatedValue, AliasAttrs>;

  MapType AttrMap;

public:
  using const_iterator = MapType::const_iterator;

  bool add(InstantiatedValue V, AliasAttrs Attr) {
    auto &OldAttr = AttrMap[V];
    auto NewAttr = OldAttr | Attr;
    if (OldAttr == NewAttr)
      return false;
    OldAttr = NewAttr;
    return true;
  }

  AliasAttrs getAttrs(InstantiatedValue V) const {
    AliasAttrs Attr;
    auto Itr = AttrMap.find(V);
    if (Itr != AttrMap.end())
      Attr = Itr->second;
    return Attr;
  }

  iterator_range<const_iterator> mappings() const {
    return make_range<const_iterator>(AttrMap.begin(), AttrMap.end());
  }
};

struct WorkListItem {
  InstantiatedValue From;
  InstantiatedValue To;
  MatchState State;
};

struct OffsetValue {
  const Value *Val;
  int64_t Offset;

  bool operator<(const OffsetValue &Other) const {
    if (Val != Other.Val)
      return std::less<const Value *>()(Val, Other.Val);
    return Offset < Other.Offset;
  }
};

// Structure to hold Anders-style analysis results
struct AndersResult {
  ReachabilitySet ReachSet;
  AliasAttrMap AttrMap;
  // Map a value to other values that may alias it (for top-level values only)
  DenseMap<const Value *, std::vector<OffsetValue>> AliasMap;
  // Map a value to its corresponding AliasAttrs (for top-level values only)
  DenseMap<const Value *, AliasAttrs> ValueAttrMap;
};

/// Mirrors the filter used by CFLSteens/Anders to avoid over-merging constants.
static bool canSkipAddingToSets(Value *Val) {
  if (isa<Constant>(Val)) {
    bool CanStoreMutableData = isa<GlobalValue>(Val) || isa<ConstantExpr>(Val) ||
                               isa<ConstantAggregate>(Val);
    return !CanStoreMutableData;
  }
  return false;
}

/// Merge a per-function CFLGraph into a module-wide graph.
static void mergeGraphInto(const CFLGraph &Src, CFLGraph &Dst) {
  for (const auto &Mapping : Src.value_mappings()) {
    Value *Val = Mapping.first;
    const auto &ValueInfo = Mapping.second;

    for (unsigned Level = 0, E = ValueInfo.getNumLevels(); Level < E; ++Level) {
      auto Node = InstantiatedValue{Val, Level};
      const auto &Info = ValueInfo.getNodeInfoAtLevel(Level);
      Dst.addNode(Node, Info.Attr);
      for (const auto &Edge : Info.Edges)
        Dst.addEdge(Node, Edge.Other, Edge.Offset);
    }
  }
}

/// Check if a function signature matches a call site's signature.
static bool matchesFunctionSignature(CallBase &CB, Function &F) {
  // Skip declarations and intrinsics
  if (F.isDeclaration() || F.isIntrinsic())
    return false;

  // Skip vararg functions for simplicity
  if (F.isVarArg())
    return false;

  // Check argument count
  if (F.arg_size() != CB.arg_size())
    return false;

  // Check return type
  if (F.getReturnType() != CB.getType())
    return false;

  // Check argument types
  auto *AI = CB.arg_begin();
  for (Function::arg_iterator FI = F.arg_begin(), FE = F.arg_end();
       FI != FE; ++FI, ++AI) {
    if (FI->getType() != (*AI)->getType())
      return false;
  }

  return true;
}

/// Get all possible target functions for an indirect call.
static void getIndirectCallTargets(CallBase &CB, Module &M,
                                   SmallVectorImpl<Function *> &Targets) {
  Type *CalledTy = CB.getCalledOperand()->getType();
  if (!CalledTy->isPointerTy())
    return;

  // Verify it's a function pointer type
  if (!isa<FunctionType>(CalledTy->getPointerElementType()))
    return;

  // Iterate through all functions in the module to find matching signatures
  for (Function &F : M) {
    // For indirect calls, we conservatively consider all functions with matching
    // signatures. Functions without address taken could still be called indirectly
    // in some cases (e.g., through casts), but requiring hasAddressTaken() is a
    // common heuristic to reduce false positives.
    // 
    // For now, we'll be conservative and check all matching functions.
    // Users can refine this by adding: if (!F.hasAddressTaken()) continue;

    // Check if function signature matches
    if (matchesFunctionSignature(CB, F))
      Targets.push_back(&F);
  }
}

/// Wire call edges (actuals -> formals, returns -> call site).
/// Handles both direct and indirect calls.
static void addCallEdges(Module &M, CFLGraph &G) {
  for (Function &F : M) {
    for (BasicBlock &BB : F) {
      for (Instruction &I : BB) {
        auto *CB = dyn_cast<CallBase>(&I);
        if (!CB)
          continue;

        SmallVector<Function *, 4> Callees;

        // Handle direct calls
        Function *DirectCallee = CB->getCalledFunction();
        if (DirectCallee && !DirectCallee->isDeclaration()) {
          Callees.push_back(DirectCallee);
        } else {
          // Handle indirect calls: find all possible targets
          getIndirectCallTargets(*CB, M, Callees);
        }

        // Process each possible callee
        for (Function *Callee : Callees) {
          if (!Callee || Callee->isDeclaration())
            continue;

          // Add argument edges: actuals -> formals
          unsigned ArgNo = 0;
          for (auto AI = Callee->arg_begin(), AE = Callee->arg_end();
               AI != AE && ArgNo < CB->arg_size(); ++AI, ++ArgNo) {
            Value *Actual = CB->getArgOperand(ArgNo);
            Argument &Formal = *AI;
            if (!Actual->getType()->isPointerTy() || !Formal.getType()->isPointerTy())
              continue;

            G.addNode(InstantiatedValue{Actual, 0});
            G.addNode(InstantiatedValue{&Formal, 0},
                      getGlobalOrArgAttrFromValue(Formal));
            G.addEdge(InstantiatedValue{Actual, 0}, InstantiatedValue{&Formal, 0});
          }

          // Add return edges: returns -> call site
          if (CB->getType()->isPointerTy()) {
            for (BasicBlock &CalBB : *Callee) {
              auto *RI = dyn_cast<ReturnInst>(CalBB.getTerminator());
              if (!RI)
                continue;
              Value *RetV = RI->getReturnValue();
              if (!RetV || !RetV->getType()->isPointerTy())
                continue;
              G.addNode(InstantiatedValue{RetV, 0});
              G.addNode(InstantiatedValue{CB, 0});
              G.addEdge(InstantiatedValue{RetV, 0}, InstantiatedValue{CB, 0});
            }
          }
        }
      }
    }
  }
}

/// Populate a StratifiedSetsBuilder from a graph.
static void fillSetsFromGraph(const CFLGraph &Graph,
                              StratifiedSetsBuilder<InstantiatedValue> &SB) {
  for (const auto &Mapping : Graph.value_mappings()) {
    Value *Val = Mapping.first;
    if (canSkipAddingToSets(Val))
      continue;
    const auto &ValueInfo = Mapping.second;
    unsigned Levels = ValueInfo.getNumLevels();
    if (!Levels)
      continue;

    SB.add(InstantiatedValue{Val, 0});
    SB.noteAttributes(InstantiatedValue{Val, 0},
                      ValueInfo.getNodeInfoAtLevel(0).Attr);
    for (unsigned L = 1; L < Levels; ++L) {
      SB.add(InstantiatedValue{Val, L});
      SB.noteAttributes(InstantiatedValue{Val, L},
                        ValueInfo.getNodeInfoAtLevel(L).Attr);
      SB.addBelow(InstantiatedValue{Val, L - 1}, InstantiatedValue{Val, L});
    }
  }

  for (const auto &Mapping : Graph.value_mappings()) {
    Value *Val = Mapping.first;
    if (canSkipAddingToSets(Val))
      continue;
    const auto &ValueInfo = Mapping.second;
    unsigned Levels = ValueInfo.getNumLevels();
    for (unsigned L = 0; L < Levels; ++L) {
      auto Src = InstantiatedValue{Val, L};
      for (const auto &Edge : ValueInfo.getNodeInfoAtLevel(L).Edges)
        SB.addWith(Src, Edge.Other);
    }
  }
}

// Helper functions for Anders-style processing
static void propagate(InstantiatedValue From, InstantiatedValue To,
                      MatchState State, ReachabilitySet &ReachSet,
                      std::vector<WorkListItem> &WorkList) {
  if (From == To)
    return;
  if (ReachSet.insert(From, To, State))
    WorkList.push_back(WorkListItem{From, To, State});
}

static void initializeWorkList(std::vector<WorkListItem> &WorkList,
                               ReachabilitySet &ReachSet,
                               const CFLGraph &Graph) {
  for (const auto &Mapping : Graph.value_mappings()) {
    auto *Val = Mapping.first;
    auto &ValueInfo = Mapping.second;
    assert(ValueInfo.getNumLevels() > 0);

    // Insert all immediate assignment neighbors to the worklist
    for (unsigned I = 0, E = ValueInfo.getNumLevels(); I < E; ++I) {
      auto Src = InstantiatedValue{Val, I};
      // If there's an assignment edge from X to Y, it means Y is reachable from
      // X at S3 and X is reachable from Y at S1
      for (auto &Edge : ValueInfo.getNodeInfoAtLevel(I).Edges) {
        propagate(Edge.Other, Src, MatchState::FlowFromReadOnly, ReachSet,
                  WorkList);
        propagate(Src, Edge.Other, MatchState::FlowToWriteOnly, ReachSet,
                  WorkList);
      }
    }
  }
}

static Optional<InstantiatedValue> getNodeBelow(const CFLGraph &Graph,
                                                InstantiatedValue V) {
  auto NodeBelow = InstantiatedValue{V.Val, V.DerefLevel + 1};
  if (Graph.getNode(NodeBelow))
    return NodeBelow;
  return None;
}

static void processWorkListItem(const WorkListItem &Item, const CFLGraph &Graph,
                                ReachabilitySet &ReachSet, AliasMemSet &MemSet,
                                std::vector<WorkListItem> &WorkList) {
  auto FromNode = Item.From;
  auto ToNode = Item.To;

  const auto *NodeInfo = Graph.getNode(ToNode);
  assert(NodeInfo != nullptr);

  // The newly added value alias pair may potentially generate more memory
  // alias pairs. Check for them here.
  auto FromNodeBelow = getNodeBelow(Graph, FromNode);
  auto ToNodeBelow = getNodeBelow(Graph, ToNode);
  if (FromNodeBelow && ToNodeBelow &&
      MemSet.insert(*FromNodeBelow, *ToNodeBelow)) {
    propagate(*FromNodeBelow, *ToNodeBelow,
              MatchState::FlowFromMemAliasNoReadWrite, ReachSet, WorkList);
    for (const auto &Mapping : ReachSet.reachableValueAliases(*FromNodeBelow)) {
      auto Src = Mapping.first;
      auto MemAliasPropagate = [&](MatchState FromState, MatchState ToState) {
        if (Mapping.second.test(static_cast<size_t>(FromState)))
          propagate(Src, *ToNodeBelow, ToState, ReachSet, WorkList);
      };

      MemAliasPropagate(MatchState::FlowFromReadOnly,
                        MatchState::FlowFromMemAliasReadOnly);
      MemAliasPropagate(MatchState::FlowToWriteOnly,
                        MatchState::FlowToMemAliasWriteOnly);
      MemAliasPropagate(MatchState::FlowToReadWrite,
                        MatchState::FlowToMemAliasReadWrite);
    }
  }

  // This is the core of the state machine walking algorithm.
  auto NextAssignState = [&](MatchState State) {
    for (const auto &AssignEdge : NodeInfo->Edges)
      propagate(FromNode, AssignEdge.Other, State, ReachSet, WorkList);
  };
  auto NextRevAssignState = [&](MatchState State) {
    for (const auto &RevAssignEdge : NodeInfo->ReverseEdges)
      propagate(FromNode, RevAssignEdge.Other, State, ReachSet, WorkList);
  };
  auto NextMemState = [&](MatchState State) {
    if (const auto *AliasSet = MemSet.getMemoryAliases(ToNode)) {
      for (const auto &MemAlias : *AliasSet)
        propagate(FromNode, MemAlias, State, ReachSet, WorkList);
    }
  };

  switch (Item.State) {
  case MatchState::FlowFromReadOnly:
    NextRevAssignState(MatchState::FlowFromReadOnly);
    NextAssignState(MatchState::FlowToReadWrite);
    NextMemState(MatchState::FlowFromMemAliasReadOnly);
    break;

  case MatchState::FlowFromMemAliasNoReadWrite:
    NextRevAssignState(MatchState::FlowFromReadOnly);
    NextAssignState(MatchState::FlowToWriteOnly);
    break;

  case MatchState::FlowFromMemAliasReadOnly:
    NextRevAssignState(MatchState::FlowFromReadOnly);
    NextAssignState(MatchState::FlowToReadWrite);
    break;

  case MatchState::FlowToWriteOnly:
    NextAssignState(MatchState::FlowToWriteOnly);
    NextMemState(MatchState::FlowToMemAliasWriteOnly);
    break;

  case MatchState::FlowToReadWrite:
    NextAssignState(MatchState::FlowToReadWrite);
    NextMemState(MatchState::FlowToMemAliasReadWrite);
    break;

  case MatchState::FlowToMemAliasWriteOnly:
    NextAssignState(MatchState::FlowToWriteOnly);
    break;

  case MatchState::FlowToMemAliasReadWrite:
    NextAssignState(MatchState::FlowToReadWrite);
    break;
  }
}

static AliasAttrMap buildAttrMap(const CFLGraph &Graph,
                                 const ReachabilitySet &ReachSet) {
  AliasAttrMap AttrMap;
  std::vector<InstantiatedValue> WorkList, NextList;

  // Initialize each node with its original AliasAttrs in CFLGraph
  for (const auto &Mapping : Graph.value_mappings()) {
    auto *Val = Mapping.first;
    auto &ValueInfo = Mapping.second;
    for (unsigned I = 0, E = ValueInfo.getNumLevels(); I < E; ++I) {
      auto Node = InstantiatedValue{Val, I};
      AttrMap.add(Node, ValueInfo.getNodeInfoAtLevel(I).Attr);
      WorkList.push_back(Node);
    }
  }

  while (!WorkList.empty()) {
    for (const auto &Dst : WorkList) {
      auto DstAttr = AttrMap.getAttrs(Dst);
      if (DstAttr.none())
        continue;

      // Propagate attr on the same level
      for (const auto &Mapping : ReachSet.reachableValueAliases(Dst)) {
        auto Src = Mapping.first;
        if (AttrMap.add(Src, DstAttr))
          NextList.push_back(Src);
      }

      // Propagate attr to the levels below
      auto DstBelow = getNodeBelow(Graph, Dst);
      while (DstBelow) {
        if (AttrMap.add(*DstBelow, DstAttr)) {
          NextList.push_back(*DstBelow);
          break;
        }
        DstBelow = getNodeBelow(Graph, *DstBelow);
      }
    }
    WorkList.swap(NextList);
    NextList.clear();
  }

  return AttrMap;
}

static void populateAliasMap(DenseMap<const Value *, std::vector<OffsetValue>> &AliasMap,
                             const ReachabilitySet &ReachSet) {
  for (const auto &OuterMapping : ReachSet.value_mappings()) {
    // AliasMap only cares about top-level values
    if (OuterMapping.first.DerefLevel > 0)
      continue;

    auto *Val = OuterMapping.first.Val;
    auto &AliasList = AliasMap[Val];
    for (const auto &InnerMapping : OuterMapping.second) {
      // Again, AliasMap only cares about top-level values
      if (InnerMapping.first.DerefLevel == 0)
        AliasList.push_back(OffsetValue{InnerMapping.first.Val, UnknownOffset});
    }

    // Sort AliasList for faster lookup
    llvm::sort(AliasList);
  }
}

static void populateValueAttrMap(DenseMap<const Value *, AliasAttrs> &ValueAttrMap,
                                const AliasAttrMap &AMap) {
  for (const auto &Mapping : AMap.mappings()) {
    auto IVal = Mapping.first;
    // ValueAttrMap only cares about top-level values
    if (IVal.DerefLevel == 0)
      ValueAttrMap[IVal.Val] |= Mapping.second;
  }
}

} // end anonymous namespace

CFLModuleAAResult::~CFLModuleAAResult() {
  if (AndersData) {
    delete static_cast<AndersResult*>(AndersData);
    AndersData = nullptr;
  }
}

char CFLModuleAA::ID = 0;

CFLModuleAA::CFLModuleAA() : ModulePass(ID), Result(CFLModuleAAAlgorithm::Steens) {}

CFLModuleAA::CFLModuleAA(CFLModuleAAAlgorithm Algo) : ModulePass(ID), Result(Algo) {}

static RegisterPass<CFLModuleAA>
    X("cfl-module-aa", "CFL Module Alias Analysis", false, true);

void CFLModuleAA::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  AU.addRequired<TargetLibraryInfoWrapperPass>();
}

bool CFLModuleAA::runOnModule(Module &M) {
  EmptySummaryProvider SummaryProvider;
  CFLGraph GlobalGraph;

  auto &TLIWP = getAnalysis<TargetLibraryInfoWrapperPass>();

  for (Function &F : M) {
    if (F.isDeclaration())
      continue;
    CFLGraphBuilder<EmptySummaryProvider> Builder(SummaryProvider,
                                                  TLIWP.getTLI(F), F);
    mergeGraphInto(Builder.getCFLGraph(), GlobalGraph);
  }

  addCallEdges(M, GlobalGraph);

  if (Result.Algorithm == CFLModuleAAAlgorithm::Steens) {
    // Steens-style: Build StratifiedSets directly
    StratifiedSetsBuilder<InstantiatedValue> SetBuilder;
    fillSetsFromGraph(GlobalGraph, SetBuilder);
    Result.Sets = SetBuilder.build();
  } else {
    // Anders-style: Perform reachability analysis
    ReachabilitySet ReachSet;
    AliasMemSet MemSet;
    
    std::vector<WorkListItem> WorkList, NextList;
    initializeWorkList(WorkList, ReachSet, GlobalGraph);
    
    while (!WorkList.empty()) {
      for (const auto &Item : WorkList)
        processWorkListItem(Item, GlobalGraph, ReachSet, MemSet, NextList);
      
      NextList.swap(WorkList);
      NextList.clear();
    }
    
    // Build attribute map
    auto AttrMap = buildAttrMap(GlobalGraph, ReachSet);
    
    // Build top-level value maps for efficient querying
    DenseMap<const Value *, std::vector<OffsetValue>> AliasMap;
    DenseMap<const Value *, AliasAttrs> ValueAttrMap;
    populateAliasMap(AliasMap, ReachSet);
    populateValueAttrMap(ValueAttrMap, AttrMap);
    
    // Store results in AndersResult structure
    auto AndersData = std::make_unique<AndersResult>();
    AndersData->ReachSet = std::move(ReachSet);
    AndersData->AttrMap = std::move(AttrMap);
    AndersData->AliasMap = std::move(AliasMap);
    AndersData->ValueAttrMap = std::move(ValueAttrMap);
    
    Result.AndersData = AndersData.release();
  }
  
  return false;
}

AliasResult CFLModuleAAResult::alias(const Value *V1, const Value *V2,
                                     AAQueryInfo &) const {
  if (!V1 || !V2 || !V1->getType()->isPointerTy() || !V2->getType()->isPointerTy())
    return AliasResult::NoAlias;

  if (Algorithm == CFLModuleAAAlgorithm::Steens) {
    // Steens-style query using StratifiedSets
    auto MaybeA = Sets.find(InstantiatedValue{const_cast<Value *>(V1), 0});
    auto MaybeB = Sets.find(InstantiatedValue{const_cast<Value *>(V2), 0});
    if (!MaybeA.hasValue() || !MaybeB.hasValue())
      return AliasResult::MayAlias;

    auto SetA = *MaybeA;
    auto SetB = *MaybeB;
    auto AttrsA = Sets.getLink(SetA.Index).Attrs;
    auto AttrsB = Sets.getLink(SetB.Index).Attrs;

    if (SetA.Index == SetB.Index)
      return AliasResult::MayAlias;
    if (AttrsA.none() || AttrsB.none())
      return AliasResult::NoAlias;
    if (hasUnknownOrCallerAttr(AttrsA) || hasUnknownOrCallerAttr(AttrsB))
      return AliasResult::MayAlias;
    if (isGlobalOrArgAttr(AttrsA) && isGlobalOrArgAttr(AttrsB))
      return AliasResult::MayAlias;
    return AliasResult::NoAlias;
  } else {
    // Anders-style query using ReachabilitySet and attribute map
    assert(AndersData && "Anders data not initialized");
    auto *AD = static_cast<AndersResult*>(AndersData);
    
    // Check if we've seen V1 and V2 before
    auto ItrA = AD->ValueAttrMap.find(V1);
    auto ItrB = AD->ValueAttrMap.find(V2);
    if (ItrA == AD->ValueAttrMap.end() || 
        ItrB == AD->ValueAttrMap.end())
      return AliasResult::MayAlias;
    
    auto AttrsA = ItrA->second;
    auto AttrsB = ItrB->second;
    
    // Check AliasAttrs before AliasMap lookup since it's cheaper
    if (hasUnknownOrCallerAttr(AttrsA))
      return AttrsB.any() ? AliasResult::MayAlias : AliasResult::NoAlias;
    if (hasUnknownOrCallerAttr(AttrsB))
      return AttrsA.any() ? AliasResult::MayAlias : AliasResult::NoAlias;
    if (isGlobalOrArgAttr(AttrsA))
      return isGlobalOrArgAttr(AttrsB) ? AliasResult::MayAlias : AliasResult::NoAlias;
    if (isGlobalOrArgAttr(AttrsB))
      return isGlobalOrArgAttr(AttrsA) ? AliasResult::MayAlias : AliasResult::NoAlias;
    
    // At this point both V1 and V2 should point to locally allocated objects
    // Check AliasMap
    auto AliasItr = AD->AliasMap.find(V1);
    if (AliasItr != AD->AliasMap.end()) {
      // Find out all (X, Offset) where X == V2
      auto Comparator = [](OffsetValue LHS, OffsetValue RHS) {
        return std::less<const Value *>()(LHS.Val, RHS.Val);
      };
      auto RangePair = std::equal_range(AliasItr->second.begin(), AliasItr->second.end(),
                                        OffsetValue{V2, 0}, Comparator);
      if (RangePair.first != RangePair.second)
        return AliasResult::MayAlias;
    }
    
    // Also check reverse direction (since alias relation is symmetric)
    AliasItr = AD->AliasMap.find(V2);
    if (AliasItr != AD->AliasMap.end()) {
      auto Comparator = [](OffsetValue LHS, OffsetValue RHS) {
        return std::less<const Value *>()(LHS.Val, RHS.Val);
      };
      auto RangePair = std::equal_range(AliasItr->second.begin(), AliasItr->second.end(),
                                        OffsetValue{V1, 0}, Comparator);
      if (RangePair.first != RangePair.second)
        return AliasResult::MayAlias;
    }
    
    return AliasResult::NoAlias;
  }
}
