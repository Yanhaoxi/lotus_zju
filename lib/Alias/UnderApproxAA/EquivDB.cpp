/*
// This file implements a must-alias analysis using union-find with congruence
// closure. The goal is to compute an *under-approximation* of pointer 
// equivalence: if two pointers are in the same equivalence class, they are
// guaranteed to alias. We never produce false positives, but may miss aliases.
//
// ┌─────────────────────────────────────────────────────────────────────────┐
// │ DATA STRUCTURE: Union-Find with Watches                                 │
// └─────────────────────────────────────────────────────────────────────────┘
//
//   • Each Value* gets a unique ID (IdTy = unsigned)
//   • Val2Id: DenseMap<const Value*, IdTy>  — value → ID lookup
//   • Id2Val: vector<const Value*>          — ID → value lookup
//   • Nodes:  vector<{Parent, Rank}>        — union-find forest
//   • Watches: vector<SmallVector<Inst*>>   — per-class watch lists
//
//   Watches[i] stores instructions that depend on class i. When two classes
//   merge, we revisit all watched instructions to check if new semantic rules
//   can fire (e.g., a PHI becomes "closed" when all operands unify).
//
// ┌─────────────────────────────────────────────────────────────────────────┐
// │ ALGORITHM: Two-Phase Construction                                       │
// └─────────────────────────────────────────────────────────────────────────┘
//
//   Phase 1: SEED with atomic must-alias pairs
//   ──────────────────────────────────────────
//   Scan all instructions and their operands. For each pair (A, B), apply
//   local syntactic rules to check if A and B must alias:
//
//     1. Identity:           A == B (after stripping casts)
//     2. Cast equivalence:   A = bitcast(B) or A = addrspacecast(B)
//     3. Const-offset GEP:   GEP(base, C₁) == GEP(base, C₂) when C₁ ≡ C₂
//     4. Zero GEP:           GEP(p, 0, 0, ...) == p
//     5. Round-trip cast:    inttoptr(ptrtoint(p)) == p
//     6. Same object:        Both derive from same alloca/global
//     7. Constant null:      null == null (same address space)
//     8. Trivial PHI:        phi [p, bb1], [p, bb2] == p
//     9. Trivial Select:     select cond, p, p == p
//
//   All matching pairs (A, B) are added to a worklist WL.
//   Additionally, register each pointer-producing instruction I as "watched"
//   by the equivalence classes of its pointer operands.
//
//   Phase 2: PROPAGATE with semantic rules
//   ───────────────────────────────────────
//   Process the worklist WL:
//
//     while WL not empty:
//       pop (A, B) from WL
//       CA ← find(A);  CB ← find(B)
//       if CA == CB: continue  // already unified
//       
//       unite(CA, CB) → NewRoot
//       
//       // Revisit all instructions watching the merged class
//       for each instruction I in Watches[NewRoot]:
//         if I is a pointer instruction:
//           try semantic rules (e.g., closed PHI, closed Select):
//             if all pointer operands of I are now in the same class:
//               Rep ← representative of that class
//               push (I, Rep) to WL  // I must equal Rep
//
//   Semantic rules capture higher-order patterns:
//     • Closed PHI:    phi [p₁, bb₁], ..., [pₙ, bbₙ] where p₁ ≡ ... ≡ pₙ
//                      → the PHI must equal the common class
//     • Closed Select: select cond, pTrue, pFalse where pTrue ≡ pFalse
//                      → the Select must equal the common class
//
//   These rules are *inductive*: as more values unify, previously non-closed
//   instructions may become closed, enabling further propagation.
//
// ┌─────────────────────────────────────────────────────────────────────────┐
// │ QUERY: mustAlias(A, B)                                                  │
// └─────────────────────────────────────────────────────────────────────────┘
//
//   After construction, query in O(α(N)) time:
//     return find(id(A)) == find(id(B))
//
//   If A and B are in the same equivalence class, they are guaranteed to
//   alias (sound under-approximation). If not, we don't know.

*/

#include "Alias/UnderApproxAA/EquivDB.h"
#include "Alias/UnderApproxAA/Canonical.h"
#include <llvm/Analysis/ValueTracking.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <queue>

using namespace llvm;
using namespace UnderApprox;


//
//===----------------------------------------------------------------------===//
//             1.  Atomic Must-Alias Rules
//===----------------------------------------------------------------------===//
//
// Each rule below checks for a specific syntactic pattern that guarantees
// two pointers must alias. All rules are conservative: they never produce
// false positives, but may miss some true aliases (under-approximation).
//
// To add a new rule:
//   1. Create a static helper function following the pattern below
//   2. Add clear documentation explaining when the rule applies
//   3. Call it from atomicMustAlias()
//===----------------------------------------------------------------------===//


/// Rule 1: Identity
/// Two pointers are the same SSA value (after stripping no-op casts).
/// Example: %p and %p
static bool checkIdentity(const Value *S1, const Value *S2) {
  return S1 == S2;
}

/// Rule 2: Bitcast/AddressSpaceCast Equivalence
/// A pointer and its bitcast/addrspacecast (when no-op) are aliases.
/// Example: %p and bitcast %p to i8*
static bool checkCastEquivalence(const Value *S1, const Value *S2) {
  if (auto *Op = dyn_cast<Operator>(S1))
    if ((isa<BitCastOperator>(Op) || isNoopAddrSpaceCast(Op)) &&
        stripNoopCasts(Op->getOperand(0)) == S2)
      return true;
  
  if (auto *Op = dyn_cast<Operator>(S2))
    if ((isa<BitCastOperator>(Op) || isNoopAddrSpaceCast(Op)) &&
        stripNoopCasts(Op->getOperand(0)) == S1)
      return true;
  
  return false;
}

/// Rule 3: Constant Offset GEP Equivalence
/// Two GEPs with the same base and identical constant offsets are aliases.
/// Example: GEP(%base, 0, i) and GEP(%base, 0, i)
static bool checkConstOffsetGEP(const DataLayout &DL, 
                                 const Value *S1, const Value *S2) {
  return sameConstOffset(DL, S1, S2);
}

/// Rule 4: Zero-Index GEP ↔ Base Pointer
/// A GEP with all zero indices is the same as its base pointer.
/// Example: GEP(%p, 0, 0) and %p
static bool checkZeroGEP(const Value *S1, const Value *S2) {
  if (isZeroGEP(S1) &&
      stripNoopCasts(cast<GEPOperator>(S1)->getPointerOperand()) == S2)
    return true;
  
  if (isZeroGEP(S2) &&
      stripNoopCasts(cast<GEPOperator>(S2)->getPointerOperand()) == S1)
    return true;
  
  return false;
}

/// Rule 5: Round-Trip Pointer ↔ Integer Cast
/// A pointer converted to integer and back (with no arithmetic) is unchanged.
/// Example: inttoptr(ptrtoint(%p)) and %p
static bool checkRoundTripCast(const Value *S1, const Value *S2) {
  return isRoundTripCast(S1, S2) || isRoundTripCast(S2, S1);
}

/// Rule 6: Same Underlying Object
/// Two pointers derived from the same alloca or global (via casts/GEPs)
/// that resolve to the same underlying object are aliases.
/// Example: bitcast(%alloca) and GEP(%alloca, 0)
static bool checkSameUnderlyingObject(const Value *S1, const Value *S2) {
  const Value *U1 = getUnderlyingObject(S1);
  const Value *U2 = getUnderlyingObject(S2);
  
  // Only consider stack allocations and globals (not heap objects)
  return U1 == U2 && (isa<AllocaInst>(U1) || isa<GlobalVariable>(U1));
}

/// Rule 7: Constant Null Pointers
/// Two null pointers in the same address space are aliases.
/// Example: null and null (both in addrspace(0))
static bool checkConstantNull(const Value *S1, const Value *S2) {
  return isa<ConstantPointerNull>(S1) && isa<ConstantPointerNull>(S2) &&
         S1->getType()->getPointerAddressSpace() ==
         S2->getType()->getPointerAddressSpace();
}

/// Rule 8: Trivial PHI Node
/// A PHI where all incoming values are the same (after stripping casts)
/// is equivalent to that common value.
/// Example: phi [%p, %bb1], [%p, %bb2] and %p
static bool checkTrivialPHI(const Value *S1, const Value *S2) {
  if (auto *PN = dyn_cast<PHINode>(S1))
    if (llvm::all_of(PN->incoming_values(), [&](const Value *V) {
          return stripNoopCasts(V) == S2;
        }))
      return true;
  
  if (auto *PN = dyn_cast<PHINode>(S2))
    if (llvm::all_of(PN->incoming_values(), [&](const Value *V) {
          return stripNoopCasts(V) == S1;
        }))
      return true;
  
  return false;
}

/// Rule 9: Trivial Select
/// A select where both branches produce the same value (after stripping casts)
/// is equivalent to that common value.
/// Example: select %cond, %p, %p and %p
static bool checkTrivialSelect(const Value *S1, const Value *S2) {
  if (auto *SI = dyn_cast<SelectInst>(S1))
    if (stripNoopCasts(SI->getTrueValue()) == S2 &&
        stripNoopCasts(SI->getFalseValue()) == S2)
      return true;
  
  if (auto *SI = dyn_cast<SelectInst>(S2))
    if (stripNoopCasts(SI->getTrueValue()) == S1 &&
        stripNoopCasts(SI->getFalseValue()) == S1)
      return true;
  
  return false;
}



//===----------------------------------------------------------------------===//
// Main atomic must-alias checker
//===----------------------------------------------------------------------===//

/// Checks if two pointers must alias using only local, syntactic rules.
/// This is the "atomic" test used to seed the union-find propagation.
/// 
/// Returns true if the pointers are guaranteed to alias, false otherwise.
/// Never produces false positives (sound under-approximation).
static bool atomicMustAlias(const DataLayout &DL,
                            const Value *A, const Value *B) {
  // Normalize by stripping no-op casts first
  const Value *S1 = stripNoopCasts(A);
  const Value *S2 = stripNoopCasts(B);

  // Apply each rule in sequence (order doesn't matter for correctness,
  // but checking cheaper rules first may improve performance)
  if (checkIdentity(S1, S2))              return true;
  if (checkCastEquivalence(S1, S2))       return true;
  if (checkConstOffsetGEP(DL, S1, S2))    return true;
  if (checkZeroGEP(S1, S2))               return true;
  if (checkRoundTripCast(S1, S2))         return true;
  if (checkSameUnderlyingObject(S1, S2))  return true;
  if (checkConstantNull(S1, S2))          return true;
  if (checkTrivialPHI(S1, S2))            return true;
  if (checkTrivialSelect(S1, S2))         return true;
  // No rule matched - cannot prove they must alias
  return false;
}

//===----------------------------------------------------------------------===//
//             2.  Union–find helpers
//===----------------------------------------------------------------------===//

EquivDB::IdTy EquivDB::id(const Value *V) {
  auto It = Val2Id.find(V);
  if (It != Val2Id.end()) return It->second;

  IdTy New = Nodes.size();
  Nodes.push_back({New, 0});
  Id2Val.push_back(V);
  Val2Id[V] = New;
  Watches.emplace_back();
  return New;
}

EquivDB::IdTy EquivDB::find(IdTy X) {
  if (Nodes[X].Parent == X) return X;
  return Nodes[X].Parent = find(Nodes[X].Parent);
}

void EquivDB::unite(IdTy A, IdTy B) {
  A = find(A); B = find(B);
  if (A == B) return;

  // union-by-rank
  if (Nodes[A].Rank < Nodes[B].Rank) std::swap(A, B);
  Nodes[B].Parent = A;
  if (Nodes[A].Rank == Nodes[B].Rank) ++Nodes[A].Rank;

  // merge watch-lists
  auto &Dst = Watches[A].Users;
  auto &Src = Watches[B].Users;
  Dst.append(Src.begin(), Src.end());
  Src.clear();
}

//===----------------------------------------------------------------------===//
//                    3. Build (seed + propagate)
//===----------------------------------------------------------------------===//

//   "Do all pointer operands of I already belong to the same class?"
//      If yes, return that representative value, otherwise nullptr.
static const Value *uniquePtrOperandClass(const Instruction *I,
                                          const EquivDB &DB) {
  const Value *Rep = nullptr;
  for (const Value *Op : I->operands())
    if (Op->getType()->isPointerTy()) {
      if (!Rep) Rep = Op;
      else if (!DB.mustAlias(Rep, Op)) return nullptr;
    }
  return Rep;                // may be nullptr
}

// (Inductive Rule) S1: Alias-closed PHI 
static bool ruleClosedPHI(const Instruction *I,
  const EquivDB &DB, const Value *&Rep) {
    auto *PN = dyn_cast<PHINode>(I);
    if (!PN) return false;
    if (const Value *Common = uniquePtrOperandClass(PN, DB)) {
        Rep = Common;
        return true;
    }
    return false;
}

// (Inductive Rule) S2: Alias-closed Select 
static bool ruleClosedSelect(const Instruction *I,
     const EquivDB &DB, const Value *&Rep) {
    auto *SI = dyn_cast<SelectInst>(I);
    if (!SI) return false;
    if (const Value *Common = uniquePtrOperandClass(SI, DB)) {
        Rep = Common;
        return true;
    }
    return false;
}

// More rules to be added here...

//-----------------------------------------------------------------------
// Dispatcher table
//-----------------------------------------------------------------------
using RuleTy = bool (*)(const Instruction *, const EquivDB &, const Value *&);
static constexpr RuleTy SemanticRules[] = {
      ruleClosedPHI,
      ruleClosedSelect,
};


EquivDB::EquivDB(Function &Func)
    : DL(Func.getParent()->getDataLayout()), F(Func) {

  std::vector<std::pair<const Value *, const Value *>> WorkList;
  seedAtomicEqualities(WorkList);
  propagate(WorkList);
}

void EquivDB::seedAtomicEqualities(
    std::vector<std::pair<const Value *, const Value *>> &WL) {
  auto push = [&](const Value *A, const Value *B) {
    if (!A->getType()->isPointerTy() || !B->getType()->isPointerTy()) return;
    if (atomicMustAlias(DL, A, B)) WL.emplace_back(A, B);
  };

  for (BasicBlock &BB : F)
    for (Instruction &I : BB) {
      // result ↔ operand
      for (Value *Op : I.operands())
        push(&I, Op);

      // operand ↔ operand (needed for phi/select seeding)
      for (unsigned i = 0, e = I.getNumOperands(); i < e; ++i)
        for (unsigned j = i + 1; j < e; ++j)
          push(I.getOperand(i), I.getOperand(j));

      // Register every pointer-producing instruction as “watched” by its
      // operand classes so that we can re-check it later.
      if (!I.getType()->isPointerTy()) continue;
      for (Value *Op : I.operands())
        if (Op->getType()->isPointerTy())
          registerWatch(Op, &I);
    }
}

void EquivDB::registerWatch(const Value *Op, Instruction *I) {
  IdTy C = find(id(Op));
  auto &Vec = Watches[C].Users;
  Vec.push_back(I);
}

/// True iff all pointer operands of I are already in the same UF-class.
bool EquivDB::operandsInSameClass(const Instruction *I) const {
  IdTy Root = 0;
  bool HaveRoot = false;
  for (const Value *Op : I->operands())
    if (Op->getType()->isPointerTy()) {
      IdTy Cur = const_cast<EquivDB *>(this)->find(
          const_cast<EquivDB *>(this)->id(Op));
      if (!HaveRoot) { Root = Cur; HaveRoot = true; }
      else if (Root != Cur) return false;
    }
  return true;
}

void EquivDB::propagate(
    std::vector<std::pair<const Value *, const Value *>> &WL) {

  while (!WL.empty()) {
    const Value *A = WL.back().first;
    const Value *B = WL.back().second;
    WL.pop_back();

    IdTy CA = find(id(A));
    IdTy CB = find(id(B));
    if (CA == CB) continue;

    unite(CA, CB);
    IdTy NewRoot = find(CA);

    auto Revisit = [&](IdTy Cls) {
      auto &List = Watches[Cls].Users;
      for (Instruction *I : List) {
        if (!I->getType()->isPointerTy()) continue;
    
        const Value *Rep = nullptr;
        for (RuleTy R : SemanticRules)
          if (R(I, *this, Rep)) {        // *this == EquivDB
            WL.emplace_back(I, Rep);
            break;                       // one rule firing is enough
          }
      }
      // keep List – every instruction may fire at most once
    };

    Revisit(NewRoot); // merged list lives here
  }
}

//===----------------------------------------------------------------------===//
//                               Query
//===----------------------------------------------------------------------===//

bool EquivDB::mustAlias(const Value *A, const Value *B) const {
  auto It1 = Val2Id.find(A);
  auto It2 = Val2Id.find(B);
  if (It1 == Val2Id.end() || It2 == Val2Id.end()) return false;
  return const_cast<EquivDB *>(this)->find(It1->second) ==
         const_cast<EquivDB *>(this)->find(It2->second);
}