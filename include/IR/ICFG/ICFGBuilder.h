

#pragma once
 
#include "IR/ICFG/ICFG.h"
 
 
 class ICFGBuilder
 {
 private:
     ICFG* icfg;
 
 public:
 
     ICFGBuilder(ICFG* i): icfg(i) {}
     void build(llvm::Module* module);
 
     bool _removeCycleAfterBuild = false;
 
 public:
 
     void setRemoveCycleAfterBuild(bool b);
 
 private:
     /// Create edges between ICFG nodes within a function
     ///@{
     void processFunction(const llvm::Function* func);
     //@}
 
     /// Add and get IntraBlock ICFGNode
     IntraBlockNode* getOrAddIntraBlockICFGNode(const llvm::BasicBlock* bb)
     {
         return icfg->getIntraBlockNode(bb);
     }
 
     /// Remove inter-procedural cycle
     void removeInterCallCycle();
     /// Remove intra-procedural cycle
     void removeIntraBlockCycle();
 };
 
