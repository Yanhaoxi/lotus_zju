#pragma once

 #include <llvm/IR/Module.h>
 #include <llvm/IR/Function.h>
 #include <llvm/Support/raw_ostream.h>
 #include <llvm/IR/IRBuilder.h>
 #include <llvm/Analysis/CFG.h>
 
 #include <iostream>
 
 #include "IR/ICFG/ICFGEdge.h"
 #include "LLVMUtils/GenericGraph.h"
 

 
 class ICFGNode;
 
 /*!
  * Interprocedural control-flow graph node.
  */
 typedef GenericNode<ICFGNode, ICFGEdge> GenericICFGNodeTy;
 
 class ICFGNode : public GenericICFGNodeTy
 {
 
 public:
     /// kinds of ICFG node
     enum ICFGNodeK
     {
         IntraBlock, FunEntryBlock, FunRetBlock
     };
 
 public:
     /// Constructor
     ICFGNode(NodeID i, ICFGNodeK k) : GenericICFGNodeTy(i, k), _function(nullptr), _basic_block(nullptr)
     {
 
     }
 
     /// Return the function of this ICFGNode
     virtual const llvm::Function* getFunction() const
     {
         return _function;
     }
 
     /// Return the function of this ICFGNode
     virtual const llvm::BasicBlock* getBasicBlock() const
     {
         return _basic_block;
     }
 
     /// Overloading operator << for dumping ICFG node ID
     //@{
     friend llvm::raw_ostream &operator<<(llvm::raw_ostream &o, const ICFGNode &node)
     {
         o << node.toString();
         return o;
     }
     //@}
 
     virtual std::string toString() const;
 
     void dump() const;
 
 protected:
     const llvm::Function* _function;
     const llvm::BasicBlock* _basic_block;
 };
 
 /*!
  * ICFG node stands for a basic block
  */
 class IntraBlockNode : public ICFGNode
 {
 
 public:
     IntraBlockNode(NodeID id, const llvm::BasicBlock* bb) : ICFGNode(id, IntraBlock)
     {
         _basic_block = bb;
         _function = bb->getParent();
     }
 
     /// Methods for support type inquiry through isa, cast, and dyn_cast:
     //@{
     static inline bool classof(const IntraBlockNode*)
     {
         return true;
     }
 
     static inline bool classof(const ICFGNode *node)
     {
         return node->getNodeKind() == IntraBlock;
     }
 
     static inline bool classof(const GenericICFGNodeTy *node)
     {
         return node->getNodeKind() == IntraBlock;
     }
     //@}
 
     std::string toString() const;
 };
 
 ///*!
 // * Function entry ICFGNode containing a set of FormalParmVFGNodes of a function
 // */
 //class FunEntryBlockNode : public ICFGNode
 //{
 //public:
 //    FunEntryBlockNode(NodeID id, const llvm::BasicBlock* bb) : ICFGNode(id, FunEntryBlock)
 //    {
 //        _basic_block = bb;
 //        _function = bb->getParent();
 //    }
 //
 //    ///Methods for support type inquiry through isa, cast, and dyn_cast:
 //    //@{
 //    static inline bool classof(const FunEntryBlockNode *)
 //    {
 //        return true;
 //    }
 //
 //    static inline bool classof(const ICFGNode *node)
 //    {
 //        return node->getNodeKind() == FunEntryBlock;
 //    }
 //
 //    static inline bool classof(const GenericICFGNodeTy *node)
 //    {
 //        return node->getNodeKind() == FunEntryBlock;
 //    }
 //    //@}
 //
 //    virtual std::string toString() const;
 //};
 //
 ///*!
 // * Function return ICFGNode containing (at most one) FormalRetVFGNodes of a function
 // */
 //class RetBlockNode : public ICFGNode
 //{
 //public:
 //    RetBlockNode(NodeID id, const llvm::BasicBlock* bb) : ICFGNode(id, FunRetBlock)
 //    {
 //        _basic_block = bb;
 //        _function = bb->getParent();
 //    }
 //
 //    ///Methods for support type inquiry through isa, cast, and dyn_cast:
 //    //@{
 //    static inline bool classof(const RetBlockNode *)
 //    {
 //        return true;
 //    }
 //
 //    static inline bool classof(const ICFGNode *node)
 //    {
 //        return node->getNodeKind() == FunRetBlock;
 //    }
 //
 //    static inline bool classof(const GenericICFGNodeTy *node)
 //    {
 //        return node->getNodeKind() == FunRetBlock;
 //    }
 //    //@}
 //
 //    virtual std::string toString() const;
 //};
 
