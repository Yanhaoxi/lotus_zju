/**
 * @file CacheSpecuAnalysis.h
 * @brief Cache Timing and Speculative Execution Analysis
 *
 * This file provides cache modeling and speculative execution analysis to
 * detect cache-based side channels and speculative execution vulnerabilities
 * (Spectre-class).
 *
 * Key Features:
 * - Cache hit/miss modeling for memory accesses
 * - Speculative execution path analysis
 * - Cache timing side-channel detection
 * - Multi-branch speculation simulation
 * - Cache state propagation
 *
 * @author Lotus Analysis Framework
 * @date 2025
 * @ingroup Spectre
 */

#ifndef CACHE_SPECU_ANALYSIS_H
#define CACHE_SPECU_ANALYSIS_H

#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/ValueMap.h"

#include <map>

#include <llvm/IR/InstVisitor.h>
#include <llvm/Pass.h>

/// Default number of cache lines
#define CACHE_LINE_NUM 32
/// Default cache line size in bytes
#define CACHE_LINE_SIZE 16
/// Architecture pointer size (64-bit)
#define ARCH_SIZE 8

using namespace std;
using namespace llvm;

namespace spectre {

/**
 * @struct Var
 * @brief Represents a variable in the cache model
 *
 * Contains information about a variable's memory layout and cache mapping.
 */
struct Var {
  Value *Val;            ///< The LLVM value representing this variable
  unsigned AddrB, AddrE; ///< Address range for set/line number calculation
  unsigned LineB, LineE; ///< Cache line range
  unsigned AgeSize;      ///< Number of cache lines this variable occupies
  unsigned AgeIndex;     ///< Starting index in the Ages vector
  Type *ty;              ///< LLVM type of the variable
  unsigned alignment;    ///< Memory alignment requirement
};

/**
 * @class CacheModel
 * @brief Models cache state and access patterns
 *
 * This class simulates cache behavior for memory accesses, tracking which
 * cache lines are occupied and their relative ages (LRU positions).
 *
 * @note Supports both must-hit and may-miss analysis modes
 * @see CacheSpecuAnalysis
 */
class CacheModel {
public:
  std::set<unsigned> cacheRecord; ///< Set of occupied cache lines
  unsigned CacheLineNum;          ///< Total number of cache lines
  unsigned CacheLineSize;         ///< Size of each cache line in bytes
  unsigned CacheSetNum;           ///< Number of cache sets
  unsigned CacheLinesPerSet;      ///< Number of lines per set (associativity)

  unsigned MaxAddr;      ///< Maximum address seen
  vector<unsigned> Ages; ///< LRU ages for each cache line
  bool MustMod;          ///< true: must-hit analysis; false: may-miss analysis
  ValueMap<Value *, Var *>
      Vars; ///< Map of values to their cache representation

  // cache hit/miss statistics
  unsigned HitCount, MissCount;           ///< Actual hit/miss counts
  unsigned SpecuHitCount, SpecuMissCount; ///< Speculative hit/miss counts

  /**
   * @brief Calculate the size of an LLVM type in bytes
   * @param ty The LLVM type to measure
   * @return Size in bytes, 0 if size cannot be determined
   */
  static unsigned GetTySize(Type *ty);

  /**
   * @brief Extract address range from a GEP instruction
   * @param I The getelementptr instruction
   * @param from Output: starting address offset
   * @param to Output: ending address offset
   * @return 1 if specific location, 0 if range, -1 on error
   */
  static int GEPInstPos(GetElementPtrInst &I, unsigned &from, unsigned &to);

  /**
   * @brief Set the maximum address for this model
   * @param addr The maximum address value
   */
  void SetMaxAddr(unsigned addr) { this->MaxAddr = addr; }

  /**
   * @brief Set the LRU ages vector
   * @param ages Vector of ages for each cache line
   */
  void SetAges(vector<unsigned> ages);

  /**
   * @brief Set the variable-to-cache mapping
   * @param vars Map from values to their cache representation
   */
  void SetVarsMap(ValueMap<Value *, Var *> &vars);

  /**
   * @brief Check if two cache models have consistent configuration
   * @param model The model to compare with
   * @return true if configurations match
   */
  bool ConfigConsistent(CacheModel *model) {
    return this->CacheLineNum == model->CacheLineNum &&
           this->CacheLineSize == model->CacheLineSize &&
           this->CacheLinesPerSet == model->CacheLinesPerSet &&
           this->CacheSetNum == model->CacheSetNum;
  }

  /**
   * @brief Check if a variable spans multiple cache lines
   * @param var The variable to check
   * @return true if the variable is partially cached
   */
  bool isVarPartiallyCached(Var *var) {
    for (int i = var->AgeIndex; i < (var->AgeIndex + var->AgeSize); ++i) {
      if (Ages[i] < CacheLineNum)
        return true;
    }
    return false;
  }

  /**
   * @brief Check if two cache models are consistent
   * @param model The model to compare with
   * @return true if models are consistent
   */
  bool CacheConsistent(CacheModel *model) {
    if (this->Vars.size() != model->Vars.size())
      return false;
    if (this->Ages.size() != model->Ages.size())
      return false;

    for (const auto &var : this->Vars) {
      Var *var1 = var.second;
      if (model->Vars.find(var.first) == model->Vars.end())
        return false;
      Var *var2 = model->Vars[var.first];

      if (!(var1->AgeIndex == var2->AgeIndex && var1->AgeSize == var2->AgeSize))
        return false;
    }
    return true;
  }

  unsigned GetAge(Value *var, unsigned offset = 0);
  unsigned SetAge(Value *var, unsigned age, unsigned offset = 0);
  unsigned SetAge(Value *var, unsigned age, unsigned b, unsigned e);

  /**
   * @brief Construct a cache model
   * @param lineSize Size of each cache line
   * @param lineNum Total number of cache lines
   * @param setNum Number of cache sets
   * @param must True for must-hit analysis, false for may-miss
   */
  CacheModel(unsigned lineSize, unsigned lineNum, unsigned setNum,
             bool must = true);

  unsigned Access(Value *var, unsigned offset = 0);
  unsigned Access(Value *var, bool force);
  unsigned LocateVar(Value *var, unsigned offset);
  unsigned AddVar(Value *var, Type *ty, unsigned alignment = 1);

  /**
   * @brief Create a fork (copy) of this cache model
   * @return Pointer to the new cache model
   */
  CacheModel *fork();

  /**
   * @brief Check equality with another model
   * @param model The model to compare
   * @return true if models are equal
   */
  bool equal(CacheModel *model);

  /**
   * @brief Merge another model into this one
   * @param mod The model to merge
   * @return Pointer to the merged model
   */
  CacheModel *merge(CacheModel *mod);

  /**
   * @brief Print the cache model state
   * @param verbose Print detailed information
   */
  void dump(bool verbose = false);

  /**
   * @brief Check if a variable is in the cache
   * @param varName Name of the variable
   * @return true if the variable is cached
   */
  bool isInCache(string varName);
};

/**
 * @struct PointerLocation
 * @brief Represents the location of a pointer value
 */
struct PointerLocation {
  Value *Dest;     ///< The destination value
  unsigned Offset; ///< Offset from the base
  PointerLocation(Value *dest, unsigned offset) : Dest(dest), Offset(offset) {};
};

/**
 * @class CacheSpecuInfo
 * @brief Holds speculative execution information for a branch
 *
 * Contains the cache models for both branches of a conditional, tracking
 * which basic blocks are speculatively executed and at what depth.
 */
class CacheSpecuInfo {
public:
  unsigned Depth;        ///< Maximum speculation depth
  BasicBlock *CauseBB;   ///< The branch causing speculation
  DominatorTree *DT;     ///< Dominator tree for the function
  bool HasElse;          ///< Whether there's an else branch
  DomTreeNode *DTIf;     ///< Dominator node for if-branch
  DomTreeNode *DTElse;   ///< Dominator node for else-branch
  BasicBlock *IfEndBB;   ///< End of if-branch speculation
  BasicBlock *ElseEndBB; ///< End of else-branch speculation
  BasicBlock *MergeBB;   ///< Where branches merge
  DomTreeNode *DTEnd;    ///< Dominator node for merge block
  CacheModel *IfModel;   ///< Cache model for if-branch
  CacheModel *ElseModel; ///< Cache model for else-branch
  unsigned Finished;     ///< 0: unfinished, 1: sim finished, 2: propagated
  unsigned IfDepth, ElseDepth;    ///< Current speculation depth for each branch
  SmallSet<BasicBlock *, 4> WLIf; ///< Worklist for if-branch
  SmallSet<BasicBlock *, 4> WLElse; ///< Worklist for else-branch

  /**
   * @brief Construct CacheSpecuInfo
   * @param Cause The causing basic block
   * @param Dt Dominator tree
   * @param If Dominator node for if-branch
   * @param Else Dominator node for else-branch
   * @param End Dominator node for merge
   * @param depth Maximum speculation depth
   * @param hasElse Whether else branch exists
   */
  CacheSpecuInfo(BasicBlock *Cause, DominatorTree *Dt, DomTreeNode *If,
                 DomTreeNode *Else, DomTreeNode *End, unsigned depth,
                 bool hasElse)
      : CauseBB(Cause), DT(Dt), DTIf(If), DTElse(Else), DTEnd(End),
        Depth(depth), HasElse(hasElse) {
    IfDepth = ElseDepth = 0;
    Finished = 0;
    IfModel = ElseModel = nullptr;
    WLIf.insert(DTIf->getBlock());
    WLElse.insert(DTElse->getBlock());
  }

  /**
   * @brief Reset the speculative info for re-analysis
   */
  void Reset() {
    Finished = 0;
    IfDepth = ElseDepth = 0;
    WLIf.clear();
    WLElse.clear();
    WLIf.insert(DTIf->getBlock());
    WLElse.insert(DTElse->getBlock());
  }

  /**
   * @brief Check if a basic block is speculatively executed
   * @param bb The basic block to check
   * @return Bitmask: 0b01 for if-branch, 0b10 for else-branch, 0b11 for both
   */
  unsigned IsSpeculative(BasicBlock *bb) {
    unsigned ret = 0;
    if (this->WLIf.count(bb) && (IfDepth < Depth))
      ret |= 0x1;
    if (this->WLElse.count(bb) && (ElseDepth < Depth))
      ret |= 0x2;
    return ret;
  }

  /**
   * @brief Check if this is a speculation entry point
   * @param bb The basic block to check
   * @return 1 for if-entry, 2 for else-entry, 0 for neither
   */
  int IsSpecuEntry(BasicBlock *bb) {
    if (DTIf->getBlock() == bb)
      return 1;
    if (DTElse->getBlock() == bb)
      return 2;
    return 0;
  }

  /**
   * @brief Add a cache model for a branch
   * @param model The cache model to add
   * @param If True for if-branch, false for else-branch
   * @param cacheUpdate Whether to merge with existing model
   * @return true if maximum depth reached
   */
  bool AddModel(CacheModel *model, bool If, bool cacheUpdate) {
    if (If) {
      if ((IfModel != nullptr) && cacheUpdate)
        IfModel->merge(model);
      else
        IfModel = model->fork();
      IfDepth++;
      return (IfDepth >= Depth);
    } else {
      if ((ElseModel != nullptr) && cacheUpdate)
        ElseModel->merge(model);
      else
        ElseModel = model->fork();
      ElseDepth++;
      return (ElseDepth >= Depth);
    }
  }

  /**
   * @brief Check if speculation is finished for both branches
   * @return true if both branches reached max depth
   */
  bool IsFinished() { return (IfDepth >= Depth) && (ElseDepth >= Depth); }

  /**
   * @brief Print the speculative execution info
   * @param verbose Print detailed information
   */
  void dump(bool verbose = false) {
    dbgs() << "Specu Execution: ";
    CauseBB->print(dbgs());
    dbgs() << "If Depth: " << IfDepth << "; Else Depth: " << ElseDepth << "\n";
    if (verbose) {
      if (IfEndBB) {
        dbgs() << "\nIf End at:";
        IfEndBB->print(dbgs());
      }
      if (ElseEndBB) {
        dbgs() << "\nElse End at:";
        ElseEndBB->print(dbgs());
      }
      if (MergeBB) {
        dbgs() << "\nMerge at:";
        MergeBB->print(dbgs());
      }
    }
  }
};

/**
 * @class CacheSpecuAnalysis
 * @brief Cache timing and speculative execution analysis pass
 *
 * This analysis models cache behavior during speculative execution to detect
 * potential side channels. It uses dominator analysis to identify speculatively
 * executed code paths and models cache state propagation through them.
 *
 * @note This is an InstVisitor-based analysis
 * @see CacheModel, CacheSpecuInfo
 */
class CacheSpecuAnalysis : public InstVisitor<CacheSpecuAnalysis> {
private:
  Function *F;
  DominatorTree *DT;
  PostDominatorTree *PDT;
  ValueMap<BasicBlock *, CacheModel *>
      cacheTrace; ///< Cache state at each block
  ValueMap<BasicBlock *, CacheModel *>
      propCacheTrace;                            ///< Propagated cache state
  ValueMap<Value *, PointerLocation *> AliasMap; ///< Alias information
  AliasAnalysis *AA;                             ///< Alias analysis results
  SmallVector<std::pair<const BasicBlock *, const BasicBlock *>, 8>
      backEdges;          ///< Loop back edges
  unsigned loopBound[20]; ///< Loop bounds
  bool runSpecu = false;  ///< Whether to run speculation
  unsigned HitSpecuDepth = 0, MissSpecuDepth = 0; ///< Speculation depths
  unsigned MergeOption = 0;                       ///< Merge strategy

  std::map<std::pair<const BasicBlock *, const BasicBlock *>, CacheModel *>
      wideningMap;
  std::map<std::pair<const BasicBlock *, const BasicBlock *>, int>
      wideningMapCount;
  vector<CacheSpecuInfo *> SpecuInfo; ///< Speculative execution info

  std::map<const BasicBlock *, int> result; ///< Analysis results
  int missNum;
  bool cacheChanged;

public:
  void dump(int mod);
  bool wideningOp(CacheModel *last, CacheModel *current);

  CacheModel *model; ///< Temporary pointer to current model

  /**
   * @brief Construct the cache analysis
   * @param F The function to analyze
   * @param DT Dominator tree
   * @param PDT Post-dominator tree
   * @param AA Alias analysis
   * @param Various analysis parameters
   */
  CacheSpecuAnalysis(Function &F, DominatorTree &DT, PostDominatorTree &PDT,
                     AliasAnalysis *AA, unsigned, unsigned, unsigned, unsigned,
                     unsigned);

  /**
   * @brief Check if an edge is a back edge (loop)
   * @param from Source basic block
   * @param to Target basic block
   * @return true if this is a back edge
   */
  inline bool IsBackEdge(BasicBlock *from, BasicBlock *to) {
    return (std::find(this->backEdges.begin(), this->backEdges.end(),
                      std::pair<const BasicBlock *, const BasicBlock *>(
                          from, to)) != this->backEdges.end());
  }

  bool SpecuSim(BasicBlock *from, BasicBlock *to, CacheModel *init = nullptr);
  unsigned GetSpecuInfo(CacheSpecuInfo *&specuInfo, BasicBlock *bb);
  BasicBlock *SpecuPropagation(BasicBlock *startBB, BasicBlock *termBB,
                               CacheModel *initModl);

  bool CacheSim(Instruction *from, Instruction *dest, Instruction *to);
  bool IsValueInCache(Instruction *inst);
  bool GetInstCacheRange(Value *inst, GlobalVariable *&GV, unsigned &offset_b,
                         unsigned &offset_e);
  unsigned IsAliasTo(Value *from, Value *&to, unsigned &offset);
  vector<Value *> GetAlias(Value *val, unsigned offset = 0);
  void InitModel();
  void InitModel(GlobalVariable *var, unsigned b, unsigned e);
  void ExtractGEPC(ConstantExpr *source, Value *&target, unsigned &offset);
  void visitAllocaInst(AllocaInst &I);
  void visitLoadInst(LoadInst &I);
  void visitBitCastInst(BitCastInst &I);
  void visitStoreInst(StoreInst &I);
  void visitCallInst(CallInst &I);
  void visitPHINode(PHINode &I);
  void visitSelectInst(SelectInst &I);
  void visitIntrinsicInst(IntrinsicInst &I);
  void visitVACopyInst(VACopyInst &I);
  void visitBranchInst(BranchInst &I);
  void visitGetElementPtrInst(GetElementPtrInst &I);
  void visitInstruction(Instruction &I);
};
} // namespace spectre
#endif