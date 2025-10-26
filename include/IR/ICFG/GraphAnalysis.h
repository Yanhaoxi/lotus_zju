
 #ifndef LT_GRAPH_ANALYSIS_H
 #define LT_GRAPH_ANALYSIS_H            
 
 #include <llvm/Analysis/LoopInfo.h>
 #include <llvm/IR/Dominators.h>
 
#include "IR/ICFG/ICFG.h"

typedef std::pair<llvm::BasicBlock*, llvm::BasicBlock*> BBEdgePair;
 
 //void writeFunctionCFGToDotFile(llvm::Function& F, std::string& fileName, bool CFGOnly = false);
 
/// find all intra block backedges in the form of <tail, header>
void findFunctionBackedgesIntra(
        llvm::Function* func,
        std::set<BBEdgePair>& res);

/// calculate all intra backedge from sourceBB
void findBackedgesFromBasicBlock(
        llvm::BasicBlock* sourceBB,
        std::set<BBEdgePair>& res);
 
 /// find all intra block backedges in the form of <intraEdge>
 void findFunctionBackedgesIntraICFG(
         ICFG* icfg,
         const llvm::Function* func,
         std::set<ICFGEdge*>& res);
 
 /// find all inter call backedge in the form of <callEdge>
 void findFunctionBackedgesInterICFG(
         ICFG* icfg,
         const llvm::Function* func,
         std::set<ICFGEdge*>& res);
 
 /// calculate shortest path from sourceBB to other BB
 std::map<llvm::BasicBlock*, uint64_t> calculateDistanceMapIntra(
         llvm::BasicBlock* sourceBB);
 
std::map<llvm::BasicBlock*, uint64_t> calculateDistanceMapIntra(
        llvm::BasicBlock* sourceBB,
        const std::set<BBEdgePair>& backEdges);
 
 /// calculate shortest path from sourceBB to other BB in an acyclic ICFG
 std::map<ICFGNode*, uint64_t> calculateDistanceMapInterICFG(
         ICFG* icfg,
         ICFGNode* sourceBB);
 
 /// calculate shortest path from sourceBB to other BB in an acyclic ICFG using a map reference
 void calculateDistanceMapInterICFGWithDistanceMap(
         ICFG* icfg,
         ICFGNode* sourceBB,
         std::map<ICFGNode*, uint64_t>& distanceMap);
 
 /// calculate shortest path from sourceBB to DestBB
 bool calculateShortestPathIntra(
         llvm::BasicBlock* sourceBB,
         llvm::BasicBlock* destBB,
         std::vector<llvm::BasicBlock*>& path);
 
 /// whether from can reach to based on DominatorTree and LoopInfo
 bool isReachableFrom(llvm::BasicBlock* from, llvm::BasicBlock* to,
                  const llvm::DominatorTree* DT, const llvm::LoopInfo* LI,
                  int& iterCount);
 

 
 #endif //LT_GRAPH_ANALYSIS_H