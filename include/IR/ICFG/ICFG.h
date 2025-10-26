#pragma once
 
 #include <llvm/IR/Module.h>
 #include <llvm/IR/Function.h>
 #include <llvm/Support/raw_ostream.h>
 #include <llvm/IR/IRBuilder.h>
 #include <llvm/Analysis/CFG.h>
 
 //#include <iostream>
 
 #include "IR/ICFG/ICFGEdge.h"
 #include "IR/ICFG/ICFGNode.h"
 #include "LLVMUtils/GenericGraph.h"
 
 /*!
  * Interprocedural Control-Flow Graph (ICFG)
  */
 typedef GenericGraph<ICFGNode,ICFGEdge> GenericICFGTy;
 class ICFG : public GenericICFGTy
 {
 
 public:
 
     typedef std::unordered_map<NodeID, ICFGNode*> ICFGNodeIDToNodeMapTy;
     typedef ICFGNodeIDToNodeMapTy::iterator iterator;
     typedef ICFGNodeIDToNodeMapTy::const_iterator const_iterator;
 
     typedef std::unordered_map<const llvm::BasicBlock*, IntraBlockNode*> blockToIntraNodeMapTy;
     typedef std::unordered_map<const llvm::Function*, IntraBlockNode*> functionToEntryIntraNodeMapTy;
 //    typedef std::unordered_map<const llvm::BasicBlock*, FunEntryBlockNode*> blockToEntryNodeMapTy;
 //    typedef std::unordered_map<const llvm::BasicBlock*, RetBlockNode*> blockToRetNodeMapTy;
 
     NodeID totalICFGNode;
 
 private:
     blockToIntraNodeMapTy blockToIntraNodeMap;
     functionToEntryIntraNodeMapTy functionToEntryIntraNodeMap;
 //    blockToEntryNodeMapTy blockToEntryNodeMap;
 //    blockToRetNodeMapTy blockToRetNodeMap;
 
 public:
     /// Constructor
     ICFG();
 
     /// Destructor
     virtual ~ICFG()
     {
     }
 
     /// Get a ICFG node
     inline ICFGNode* getICFGNode(NodeID id) const
     {
         return getGNode(id);
     }
 
     /// Whether has the ICFGNode
     inline bool hasICFGNode(NodeID id) const
     {
         return hasGNode(id);
     }
 
     /// Whether we has an edge
     //@{
     ICFGEdge* hasIntraICFGEdge(ICFGNode* src, ICFGNode* dst, ICFGEdge::ICFGEdgeK kind);
     ICFGEdge* hasInterICFGEdge(ICFGNode* src, ICFGNode* dst, ICFGEdge::ICFGEdgeK kind);
     //@}
 
     /// Get an edge according to src and dst
     ICFGEdge* getICFGEdge(const ICFGNode* src, const ICFGNode* dst, ICFGEdge::ICFGEdgeK kind);
 
     /// Get function to entry block map
     inline functionToEntryIntraNodeMapTy getFunctionEntryMap()
     {
         return functionToEntryIntraNodeMap;
     }
 
 public:
     /// Remove a SVFG edge
     inline void removeICFGEdge(ICFGEdge* edge)
     {
         edge->getDstNode()->removeIncomingEdge(edge);
         edge->getSrcNode()->removeOutgoingEdge(edge);
         delete edge;
     }
     /// Remove a ICFGNode
     inline void removeICFGNode(ICFGNode* node)
     {
         removeGNode(node);
     }
 
     /// Add control-flow edges for top level pointers
     //@{
     ICFGEdge* addIntraEdge(ICFGNode* srcNode, ICFGNode* dstNode);
     ICFGEdge* addCallEdge(ICFGNode* srcNode, ICFGNode* dstNode, const llvm::Instruction* cs);
     ICFGEdge* addRetEdge(ICFGNode* srcNode, ICFGNode* dstNode, const llvm::Instruction* cs);
     //@}
 
     /// sanitize Intra edges, verify that both nodes belong to the same function.
     inline void checkIntraEdgeParents(const ICFGNode *srcNode, const ICFGNode *dstNode)
     {
         auto* srcfun = srcNode->getFunction();
         auto* dstfun = dstNode->getFunction();
         if(srcfun != nullptr && dstfun != nullptr)
         {
             assert((srcfun == dstfun) && "src and dst nodes of an intra edge should in the same function!" );
         }
     }
 
     /// Add ICFG edge
     inline bool addICFGEdge(ICFGEdge* edge)
     {
         bool added1 = edge->getDstNode()->addIncomingEdge(edge);
         bool added2 = edge->getSrcNode()->addOutgoingEdge(edge);
         assert(added1 && added2 && "edge not added??");
         return true;
     }
 
     /// Add a ICFG node
     virtual inline void addICFGNode(ICFGNode* node)
     {
         addGNode(node->getId(), node);
     }
 
     /// Get a basic block ICFGNode
     //@{
     bool hasIntraBlockNode(const llvm::BasicBlock* bb);
     IntraBlockNode* getIntraBlockNode(const llvm::BasicBlock* bb);
 //    FunEntryBlockNode* getFunEntryBlockNode(const llvm::BasicBlock* bb);
 //    RetBlockNode* getRetBlockNode(const llvm::BasicBlock* bb);
     //@}
 
 private:
     /// Get/Add IntraBlock ICFGNode
     inline IntraBlockNode* getIntraBlockICFGNode(const llvm::BasicBlock* bb)
     {
         blockToIntraNodeMapTy::const_iterator it = blockToIntraNodeMap.find(bb);
         if (it == blockToIntraNodeMap.end())
             return nullptr;
         return it->second;
     }
     inline IntraBlockNode* addIntraBlockICFGNode(const llvm::BasicBlock* bb)
     {
         IntraBlockNode* sNode = new IntraBlockNode(totalICFGNode++, bb);
         addICFGNode(sNode);
         blockToIntraNodeMap[bb] = sNode;
 
         if (bb == &bb->getParent()->front()) {
 
             functionToEntryIntraNodeMap[bb->getParent()] = sNode;
         }
 
         return sNode;
     }
 
     /// Get/Add a function entry node
 //    inline FunEntryBlockNode* getFunEntryICFGNode(const llvm::BasicBlock* bb)
 //    {
 //        blockToEntryNodeMapTy::const_iterator it = blockToEntryNodeMap.find(bb);
 //        if (it == blockToEntryNodeMap.end())
 //            return nullptr;
 //        return it->second;
 //    }
 //    inline FunEntryBlockNode* addFunEntryICFGNode(const llvm::BasicBlock* bb)
 //    {
 //        FunEntryBlockNode* sNode = new FunEntryBlockNode(totalICFGNode++, bb);
 //        addICFGNode(sNode);
 //        blockToEntryNodeMap[bb] = sNode;
 //        return sNode;
 //    }
 //
 //    /// Get/Add a return node
 //    inline RetBlockNode* getRetICFGNode(const llvm::BasicBlock* bb)
 //    {
 //        blockToRetNodeMapTy::const_iterator it = blockToRetNodeMap.find(bb);
 //        if (it == blockToRetNodeMap.end())
 //            return nullptr;
 //        return it->second;
 //    }
 //    inline RetBlockNode* addRetICFGNode(const llvm::BasicBlock* bb)
 //    {
 //        RetBlockNode* sNode = new RetBlockNode(totalICFGNode++, bb);
 //        addICFGNode(sNode);
 //        blockToRetNodeMap[bb] = sNode;
 //        return sNode;
 //    }
 
 };
