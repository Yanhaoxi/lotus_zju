//===- SSIUtils.cpp - SSI helper classes and utilities --------------------===//
//
// Author: rainoftime
//
//===----------------------------------------------------------------------===//
/*
 * 	 Helper classes: ProgramPoint, RenamingStack, Graph, PostDominanceFrontier
 */

 #include "IR/SSI/SSI.h"

 using namespace llvm;
 
 ProgramPoint::ProgramPoint(Instruction* I, Position P) :
         I(I), P(P)
 {
 }
 
 // Two ProgramPoints are equal iff they are of the same region type and:
 //     - if they are Self, their instruction should be the same.
 //     - if not, their instructions' parents should be the same.
 bool ProgramPoint::operator==(const ProgramPoint& o) const
 {
     if (this->P != o.P) {
         return false;
     }
 
     if (this->P == ProgramPoint::Self) {
         return this->I == o.I;
     }
 
     const BasicBlock* this_I_parent = this->I->getParent();
     const BasicBlock* o_I_parent = o.I->getParent();
 
     return this_I_parent == o_I_parent;
 }
 
 bool ProgramPoint::operator!=(const ProgramPoint& o) const
 {
     return !(*this == o);
 }
 
 bool ProgramPoint::operator<(const ProgramPoint& o) const
 {
     if (this->P < o.P) {
         return true;
     }
     if (this->P > o.P) {
         return false;
     }
 
     if (this->P == ProgramPoint::Self) {
         return this->I < o.I;
     }
 
     const BasicBlock* this_I_parent = this->I->getParent();
     const BasicBlock* o_I_parent = o.I->getParent();
 
     return this_I_parent < o_I_parent;
 }
 
 bool ProgramPoint::operator>(const ProgramPoint& o) const
 {
     return !(*this == o) && !(*this < o);
 }
 
 bool ProgramPoint::not_definition_of(const Value* V) const
 {
     const Instruction* I = this->I;
     const BasicBlock* BB = I->getParent();
 
     if (I == V)
         return false;
 
    switch (this->P) {
        case ProgramPoint::In:
            // phi case
            {
                const Instruction* FirstNonPHI = BB->getFirstNonPHI();
                for (BasicBlock::const_iterator BBit = BB->begin(), 
                     BBend = (FirstNonPHI ? FirstNonPHI->getIterator() : BB->end()); 
                     BBit != BBend; ++BBit) {
 
                    const PHINode* op = cast<PHINode>(&*BBit);
 
                    if (SSIfy::is_SSIphi(op)) {
                        unsigned n = op->getNumIncomingValues();
                        unsigned i;
                        for (i = 0; i < n; ++i) {
                            if (op->getIncomingValue(i) == V) {
                                return false;
                            }
                        }
                    }
                }
            }
            break;
 
        case ProgramPoint::Out:
            // sigma case
            for (const_succ_iterator BBsuccit = succ_begin(BB), BBsuccend =
                    succ_end(BB); BBsuccit != BBsuccend; ++BBsuccit) {
                const BasicBlock* BBsucc = *BBsuccit;
 
                const Instruction* FirstNonPHI = BBsucc->getFirstNonPHI();
                for (BasicBlock::const_iterator BBit = BBsucc->begin(), 
                     BBend = (FirstNonPHI ? FirstNonPHI->getIterator() : BBsucc->end()); 
                     BBit != BBend; ++BBit) {
 
                    const PHINode* op = cast<PHINode>(&*BBit);
 
                    if (SSIfy::is_SSIsigma(op)) {
                        unsigned n = op->getNumIncomingValues();
                        unsigned i;
                        for (i = 0; i < n; ++i) {
                            if (op->getIncomingValue(i) == V) {
                                return false;
                            }
                        }
                    }
                }
            }
            break;
 
        case ProgramPoint::Self:
            // copy case
            //
            // We walk through the instructions that follow I looking for
            // a created SSI_copy that redefines V already.
            // If it exists, then there's a definition already.
            for (BasicBlock::const_iterator bit = I->getIterator(); 
                 bit != BB->end() && SSIfy::is_SSIcopy(&*bit); ++bit) {
 
                const Instruction* next = &*bit;
 
                if (SSIfy::is_SSIcopy(next)) {
                    // Check if operand is V
                    if (next->getOperand(0) == V) {
                        return false;
                    }
                }
            }
 
            break;
     }
 
     return true;
 }
 
 const DominanceFrontier::DomSetType &
 PostDominanceFrontier::calculate(const PostDominatorTree &DT,
         const DomTreeNode *Node)
 {
 // Loop over CFG successors to calculate DFlocal[Node]
     BasicBlock *BB = Node->getBlock();
     DomSetType &S = Frontiers[BB]; // The new set to fill in...
     if (getRoots().empty())
         return S;
 
     if (BB)
         for (pred_iterator SI = pred_begin(BB), SE = pred_end(BB); SI != SE;
                 ++SI) {
             BasicBlock *P = *SI;
             // Does Node immediately dominate this predecessor?
             DomTreeNode *SINode = DT[P];
             if (SINode && SINode->getIDom() != Node)
                 S.insert(P);
         }
 
 // At this point, S is DFlocal.  Now we union in DFup's of our children...
 // Loop through and visit the nodes that Node immediately dominates (Node's
 // children in the IDomTree)
 //
     for (DomTreeNode::const_iterator NI = Node->begin(), NE = Node->end();
             NI != NE; ++NI) {
         DomTreeNode *IDominee = *NI;
         const DomSetType &ChildDF = calculate(DT, IDominee);
 
         DomSetType::const_iterator CDFI = ChildDF.begin(), CDFE = ChildDF.end();
         for (; CDFI != CDFE; ++CDFI) {
             if (!DT.properlyDominates(Node, DT[*CDFI]))
                 S.insert(*CDFI);
         }
     }
 
     return S;
 }
 
 //////////////////////////////////////////////////////////////////
 
 RenamingStack::RenamingStack(Value * V)
 {
     this->V = V;
 }
 
 Value * RenamingStack::getValue() const
 {
     return this->V;
 }
 
 void RenamingStack::push(Instruction* I)
 {
     this->stack.push_back(I);
 }
 
 void RenamingStack::pop()
 {
     this->stack.pop_back();
 }
 
 Instruction * RenamingStack::peek() const
 {
     return this->stack.back();
 }
 
 bool RenamingStack::empty() const
 {
     return this->stack.empty();
 }
 
 bool ProgramPoint::is_join() const
 {
     return !this->I->getParent()->getSinglePredecessor()
             && (this->P == ProgramPoint::In);
 }
 
 bool ProgramPoint::is_branch() const
 {
     return isa<BranchInst>(this->I) && (this->P == ProgramPoint::Out);
 }
 
 bool ProgramPoint::is_copy() const
 {
     return this->P == ProgramPoint::Self;
 }
 
 void Graph::addNode(Value* V)
 {
     this->vertices[V];
 }
 
 bool Graph::hasNode(Value* V)
 {
     return this->vertices.count(V);
 }
 
 void Graph::addEdge(Value* from, Value* to)
 {
     DenseMap<Value*, SmallPtrSet<Value*, 4> >::iterator it =
             this->vertices.find(from);
 
     if (it != this->vertices.end()) {
         it->second.insert(to);
     }
 }
 
 bool Graph::hasEdge(Value* from, Value* to)
 {
     DenseMap<Value*, SmallPtrSet<Value*, 4> >::iterator it =
             this->vertices.find(from);
 
     if (it != this->vertices.end()) {
         return it->second.count(to);
     }
     else {
         return false;
     }
 }
