#pragma once
 
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Analysis/CFG.h>
 
//#include <iostream>
 
#include "LLVMUtils/GenericGraph.h"
 

 
 class ICFGNode;
 
 /*!
  * Interprocedural control-flow edge.
  */
 typedef GenericEdge<ICFGNode> GenericICFGEdgeTy;
 class ICFGEdge : public GenericICFGEdgeTy
 {
 
 public:
 
     /// ten types of ICFG edge
     /// three types of control-flow edges
     /// seven types of value-flow edges
     enum ICFGEdgeK
     {
         IntraCF,
         CallCF,
         RetCF,
     };
 
 public:
     /// Constructor
     ICFGEdge(ICFGNode* s, ICFGNode* d, ICFGEdgeK k) : GenericICFGEdgeTy(s, d, k)
     {
     }
     /// Destructor
     ~ICFGEdge()
     {
     }
 
     /// Get methods of the components
     //@{
     inline bool isCFGEdge() const
     {
         return getEdgeKind() == IntraCF || getEdgeKind() == CallCF || getEdgeKind() == RetCF;
     }
     inline bool isCallCFGEdge() const
     {
         return getEdgeKind() == CallCF;
     }
     inline bool isRetCFGEdge() const
     {
         return getEdgeKind() == RetCF;
     }
     inline bool isIntraCFGEdge() const
     {
         return getEdgeKind() == IntraCF;
     }
     //@}
 
     /// Overloading operator << for dumping ICFG node ID
     //@{
     friend llvm::raw_ostream& operator<< (llvm::raw_ostream &o, const ICFGEdge &edge)
     {
         o << edge.toString();
         return o;
     }
     //@}
 
     virtual std::string toString() const;
 };
 
 /*!
  * Intra ICFG edge representing control-flows between basic blocks within a function
  */
 class IntraCFGEdge : public ICFGEdge
 {
 
 public:
     /// Constructor
     IntraCFGEdge(ICFGNode* s, ICFGNode* d): ICFGEdge(s,d,IntraCF)
     {
     }
     /// Methods for support type inquiry through isa, cast, and dyn_cast:
     //@{
     static inline bool classof(const IntraCFGEdge *)
     {
         return true;
     }
     static inline bool classof(const ICFGEdge *edge)
     {
         return edge->getEdgeKind() == IntraCF;
     }
     static inline bool classof(const GenericICFGEdgeTy *edge)
     {
         return edge->getEdgeKind() == IntraCF;
     }
     //@}
 
     virtual std::string toString() const;
 };
 
 /*!
  * Call ICFG edge representing parameter passing/return from a caller to a callee
  */
 class CallCFGEdge : public ICFGEdge
 {
 
 private:
     const llvm::Instruction*  cs;
 public:
     /// Constructor
     CallCFGEdge(ICFGNode* s, ICFGNode* d, const llvm::Instruction*  c):
             ICFGEdge(s,d,CallCF),cs(c)
     {
     }
     /// Return callsite ID
     inline const llvm::Instruction*  getCallSite() const
     {
         return cs;
     }
     /// Methods for support type inquiry through isa, cast, and dyn_cast:
     //@{
     static inline bool classof(const CallCFGEdge *)
     {
         return true;
     }
     static inline bool classof(const ICFGEdge *edge)
     {
         return edge->getEdgeKind() == CallCF;
     }
     static inline bool classof(const GenericICFGEdgeTy *edge)
     {
         return edge->getEdgeKind() == CallCF;
     }
     //@}
     virtual std::string toString() const;
 };
 
 /*!
  * Return ICFG edge representing parameter/return passing from a callee to a caller
  */
 class RetCFGEdge : public ICFGEdge
 {
 
 private:
     const llvm::Instruction*  cs;
 public:
     /// Constructor
     RetCFGEdge(ICFGNode* s, ICFGNode* d, const llvm::Instruction*  c):
             ICFGEdge(s,d,RetCF),cs(c)
     {
     }
     /// Return callsite ID
     inline const llvm::Instruction*  getCallSite() const
     {
         return cs;
     }
     /// Methods for support type inquiry through isa, cast, and dyn_cast:
     //@{
     static inline bool classof(const RetCFGEdge *)
     {
         return true;
     }
     static inline bool classof(const ICFGEdge *edge)
     {
         return edge->getEdgeKind() == RetCF;
     }
     static inline bool classof(const GenericICFGEdgeTy *edge)
     {
         return edge->getEdgeKind() == RetCF;
     }
     //@}
     virtual std::string toString() const;
 };
 

