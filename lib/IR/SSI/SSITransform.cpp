//===- SSITransform.cpp - SSI transformation logic ------------------------===//
//
//		This file is licensed under the General Public License v2.
//
//===----------------------------------------------------------------------===//
/*
 * 	 Core SSI transformation algorithms: split, rename, and clean
 */

 #include "IR/SSI/SSI.h"

 using namespace llvm;
 
 extern cl::opt<bool> Verbose;
 
 void SSIfy::split(Instruction* V, const std::set<ProgramPoint> &Iup,
         const std::set<ProgramPoint> &Idown)
 {
     std::set<ProgramPoint> Sup;
     std::set<ProgramPoint> Sdown;
 
     if (Verbose) {
         errs() << "Splitting " << V->getName() << "\n";
     }
 
     // Creation of the Sup set. Its logic is defined in the referenced paper.
     for (std::set<ProgramPoint>::iterator sit = Iup.begin(), send = Iup.end();
             sit != send; ++sit) {
         ProgramPoint point = *sit;
         Instruction* I = point.I;
         BasicBlock* BBparent = I->getParent();
 
         if (point.is_join()) {
             for (pred_iterator PI = pred_begin(BBparent), E = pred_end(
                     BBparent); PI != E; ++PI) {
                 BasicBlock *BBpred = *PI;
 
                 SmallPtrSet<BasicBlock*, 4> iterated_pdf = get_iterated_pdf(
                         BBpred);
 
                 for (SmallPtrSet<BasicBlock*, 4>::iterator sit =
                         iterated_pdf.begin(), send = iterated_pdf.end();
                         sit != send; ++sit) {
                     BasicBlock* BB = *sit;
                     Instruction& last = BB->back();
                     Sup.insert(ProgramPoint(&last, ProgramPoint::Out));
                 }
             }
         }
         else {
             SmallPtrSet<BasicBlock*, 4> iterated_pdf = get_iterated_pdf(
                     BBparent);
 
             for (SmallPtrSet<BasicBlock*, 4>::iterator sit =
                     iterated_pdf.begin(), send = iterated_pdf.end();
                     sit != send; ++sit) {
                 BasicBlock* BB = *sit;
                 Instruction& last = BB->back();
                 Sup.insert(ProgramPoint(&last, ProgramPoint::Out));
             }
         }
     }
 
     // Union of Sup, Idown and V
     std::set<ProgramPoint> NewSet;
     NewSet.insert(Sup.begin(), Sup.end());
     NewSet.insert(Idown.begin(), Idown.end());
 
     for (std::set<ProgramPoint>::iterator sit = NewSet.begin(), send =
     // Creation of Sdown. Logic defined in the paper as well.
             NewSet.end(); sit != send; ++sit) {
         ProgramPoint point = *sit;
         Instruction* I = point.I;
         BasicBlock* BBparent = I->getParent();
 
         if (point.is_branch()) {
             for (succ_iterator PI = succ_begin(BBparent), E = succ_end(
                     BBparent); PI != E; ++PI) {
                 BasicBlock *BBsucc = *PI;
 
                 SmallPtrSet<BasicBlock*, 4> iterated_df = get_iterated_df(
                         BBsucc);
 
                 for (SmallPtrSet<BasicBlock*, 4>::iterator sit =
                         iterated_df.begin(), send = iterated_df.end();
                         sit != send; ++sit) {
                     BasicBlock* BB = *sit;
                     Instruction& first = BB->front();
                     Sdown.insert(ProgramPoint(&first, ProgramPoint::In));
                 }
             }
         }
         else {
             SmallPtrSet<BasicBlock*, 4> iterated_df = get_iterated_df(BBparent);
 
             for (SmallPtrSet<BasicBlock*, 4>::iterator sit =
                     iterated_df.begin(), send = iterated_df.end(); sit != send;
                     ++sit) {
                 BasicBlock* BB = *sit;
                 Instruction& first = BB->front();
                 Sdown.insert(ProgramPoint(&first, ProgramPoint::In));
             }
         }
     }
 
     // Finally
     std::set<ProgramPoint> S;
     S.insert(Iup.begin(), Iup.end());
     S.insert(Idown.begin(), Idown.end());
     S.insert(Sup.begin(), Sup.end());
     S.insert(Sdown.begin(), Sdown.end());
 
     /*
      * 	Split live range of v by inserting sigma, phi, and copies
      */
     for (std::set<ProgramPoint>::iterator sit = S.begin(), send = S.end();
             sit != send; ++sit) {
         const ProgramPoint &point = *sit;
 
         if (point.not_definition_of(V)) {
             Instruction* insertion_point = point.I;
             ProgramPoint::Position relative_position = point.P;
 
             // Check if new variable is actually not necessary
             // NOTE: it only checks if it is NOT necessary. That doesn't
             // mean that it is necessary if the check returns false.
             // Removing this check makes this pass 10x slower.
             if (isNotNecessary(insertion_point, V)) {
                 continue;
             }
 
             if (point.is_join()) {
                 // phi
                 unsigned numReservedValues = std::distance(
                         pred_begin(insertion_point->getParent()),
                         pred_end(insertion_point->getParent()));
                 PHINode* new_phi = PHINode::Create(V->getType(),
                         numReservedValues, phiname);
 
                 // Add V multiple times as incoming value to the new phi
                 for (pred_iterator BBit = pred_begin(
                         insertion_point->getParent()), BBend = pred_end(
                         insertion_point->getParent()); BBit != BBend; ++BBit) {
                     BasicBlock* predBB = *BBit;
                     new_phi->addIncoming(V, predBB);
                 }
 
                 switch (relative_position) {
                     case ProgramPoint::In:
                         new_phi->insertBefore(insertion_point);
                         break;
                     default:
                         errs() << "Problem here1";
                         break;
                 }
 
                 if (Verbose) {
                     errs() << "Created " << new_phi->getName() << "\n";
                 }
 
                 this->versions[V].insert(new_phi);
                 ++NumPHIsCreated;
             }
             else if (point.is_branch()) {
                 // sigma
                 // Insert one sigma in each of the successors
                 BasicBlock* BBparent = point.I->getParent();
                 unsigned numReservedValues = 1;
 
                 for (succ_iterator PI = succ_begin(BBparent), E = succ_end(
                         BBparent); PI != E; ++PI) {
                     BasicBlock *BBsucc = *PI;
 
                     PHINode* new_sigma = PHINode::Create(V->getType(),
                             numReservedValues, signame, &BBsucc->front());
                     new_sigma->addIncoming(V, BBparent);
 
                     if (Verbose) {
                         errs() << "Created " << new_sigma->getName() << "\n";
                     }
 
                     this->versions[V].insert(new_sigma);
                     ++NumSigmasCreated;
                 }
             }
             else if (point.is_copy()) {
                 // copy
 
                 // Zero value
                 ConstantInt* zero = ConstantInt::get(
                         cast<IntegerType>(V->getType()), 0);
 
                 BinaryOperator* new_copy = BinaryOperator::Create(
                         Instruction::Add, V, zero, copname);
 
                 switch (relative_position) {
                     case ProgramPoint::Self:
                         new_copy->insertAfter(insertion_point);
                         break;
                     default:
                         errs() << "Problem here2";
                         break;
                 }
 
                 if (Verbose) {
                     errs() << "Created " << new_copy->getName() << "\n";
                 }
 
                 this->versions[V].insert(new_copy);
                 ++NumCopiesCreated;
             }
         }
     }
 }
 
 void SSIfy::rename_initial(Instruction* V)
 {
     RenamingStack stack(V);
 
     BasicBlock* root = V->getParent();
 
     rename(root, stack);
 }
 
 void SSIfy::rename(BasicBlock* BB, RenamingStack& stack)
 {
     const Value* V = stack.getValue();
 
     if (Verbose) {
         errs() << "Renaming " << V->getName() << " in " << BB->getName()
                 << "\n";
     }
 
     // Iterate over all instructions in BB
     for (BasicBlock::iterator iit = BB->begin(), iend = BB->end(); iit != iend;
             ++iit) {
         Instruction* I = cast<Instruction>(&*iit);
         PHINode* phi = dyn_cast<PHINode>(I);
 
         // foreach instruction u in n that uses v
         // We do this renaming only if it is not a SSI_phi
         // because renaming in SSI_phi is done in a step afterwards
         bool has_newdef = false;
 
         // Check if I has an use of V
         // If it does, then we mark I to be a new definition of V
         // then we call set_def on it later on
         for (User::op_iterator i = I->op_begin(), e = I->op_end(); i != e;
                 ++i) {
             Value *used = *i;
 
             if (used == V) {
                 if (!is_actual(I)) {
                     has_newdef = true;
                 }
 
                 if (!is_SSIphi(I)) {
                     set_use(stack, I);
                 }
 
                 break;
             }
         }
 
         // NEW DEFINITION OF V
         // sigma, phi or copy
         if (has_newdef) {
             if (phi) {
                 set_def(stack, phi);
             }
             // copy
             else if (is_SSIcopy(I)) {
                 set_def(stack, I);
             }
         }
     }
 
     // Searches for SSI_phis in the successors to rename uses of V in them
     for (succ_iterator sit = succ_begin(BB), send = succ_end(BB); sit != send;
             ++sit) {
         BasicBlock* BBsucc = *sit;
         for (BasicBlock::iterator BBit = BBsucc->begin(), BBend =
                 BBsucc->getFirstInsertionPt(); BBit != BBend; ++BBit) {
             PHINode* phi = dyn_cast<PHINode>(&*BBit);
 
             if (phi && is_SSIphi(phi)) {
                 set_use(stack, phi, BB);
             }
         }
     }
 
     // Now call recursively for all children in the dominance tree
     DomTreeNode* domtree = this->DTmap->getNode(BB);
     if (domtree) {
         for (DomTreeNode::iterator begin = domtree->begin(), end =
                 domtree->end(); begin != end; ++begin) {
             DomTreeNodeBase<BasicBlock> *DTN_children = *begin;
             BasicBlock *BB_children = DTN_children->getBlock();
             rename(BB_children, stack);
         }
     }
 }
 
 void SSIfy::set_use(RenamingStack& stack, Instruction* inst, BasicBlock* from)
 {
     Value* V = stack.getValue();
     Instruction* popped = 0;
 
     // If the stack is initially empty,
     // renaming didn't reach the initial
     // definition of V yet, so no point
     // in renaming yet
     if (stack.empty()) {
         return;
     }
 
     // If from != null, we are dealing with a renaming
     // inside a SSI_phi.
     if (!from) {
         while (!stack.empty()) {
             popped = stack.peek();
 
             if (!this->DTmap->dominates(popped, inst)) {
                 stack.pop();
 
                 if (Verbose) {
                     errs() << "set_use: Popping " << popped->getName()
                             << " from the stack of "
                             << stack.getValue()->getName() << "\n";
                 }
             }
             else {
                 break;
             }
         }
     }
     else {
         while (!stack.empty()) {
             popped = stack.peek();
 
             if ((popped->getParent() != from)
                     && (!this->DTmap->dominates(popped, from))) {
                 stack.pop();
 
                 if (Verbose) {
                     errs() << "set_usephi: Popping " << popped->getName()
                             << " from the stack of "
                             << stack.getValue()->getName() << "\n";
                 }
             }
             else {
                 break;
             }
         }
     }
 
     // If the stack has become empty, it means that the last valid
     // definition is actually V itself, not popped. Otherwise, popped
     // would still be in stack, therefore this wouldn't be empty.
     Instruction* new_name = stack.empty() ? cast<Instruction>(V) : popped;
 
     // We shouldn't perform renaming in any of the following cases
     if ((new_name != V) && (new_name != inst)) {
         if (!from) {
 
             if (Verbose) {
                 errs() << "set_use: Renaming uses of " << V->getName() << " in "
                         << inst->getName() << " to " << new_name->getName()
                         << "\n";
             }
 
             inst->replaceUsesOfWith(V, new_name);
         }
         else {
             PHINode* phi = cast<PHINode>(inst);
             int index = phi->getBasicBlockIndex(from);
 
             if (phi->getIncomingValue(index) == V) {
 
                 if (Verbose) {
                     errs() << "set_usephi: Renaming uses of " << V->getName()
                             << " in " << inst->getName() << " to "
                             << new_name->getName() << "\n";
                 }
 
                 phi->setIncomingValue(index, new_name);
             }
         }
     }
 }
 
 void SSIfy::set_def(RenamingStack& stack, Instruction* inst)
 {
     // Note that this function *doesn't* check if inst contains
     // an use of stack.Value!
     // Verification has to be done by the user of this function
 
     if (Verbose) {
         errs() << "set_def: Pushing " << inst->getName() << " to the stack of "
                 << stack.getValue()->getName() << "\n";
     }
 
     stack.push(inst);
 }
 
 void SSIfy::clean()
 {
     /*
      This structure saves all instructions that are marked to be erased.
      We cannot simply erase on sight because of cases like this:
      [V] -> {A B C D}
      [B] -> {...}
      If we visit V's set first and then erase B, the next iteration
      would try to access B, which would have been already erased.
      Thus, erases are performed afterwards.
      */
     SmallPtrSet<Instruction*, 16> to_be_erased;
 
     // This map associates instructions - that will be removed - to Values
     // to which their uses will be renamed.
     // In other words, this map is this->versions reversed, but containing
     // only instructions that will be erased for sure.
     DenseMap<Instruction*, Instruction*> maptooldvalues;
 
     // Please note that this next for is intended to identify what
     // instructions should be erased due to being either wrong or useless.
     // The actual remotion happens after.
     for (DenseMap<Value*, SmallPtrSet<Instruction*, 4> >::iterator mit =
             this->versions.begin(), mend = this->versions.end(); mit != mend;
             ++mit) {
 
         Instruction* V = cast<Instruction>(mit->first);
         SmallPtrSet<Instruction*, 4> created_vars = mit->second;
 
         for (SmallPtrSet<Instruction*, 4>::iterator sit = created_vars.begin(),
                 send = created_vars.end(); sit != send; ++sit) {
             Instruction* newvar = *sit;
 
             // The cleaning criteria for SSI_phi has two cases
             // First: phi whose incoming values are ALL V itself.
             // Second: phi that is not dominated by V.
             if (is_SSIphi(newvar)) {
                 PHINode* ssi_phi = cast<PHINode>(newvar);
                 bool any_value_diff_V = false;
 
                 // First case: phis with all incoming values corresponding to
                 // the original value.
                 for (unsigned i = 0, n = ssi_phi->getNumIncomingValues(); i < n;
                         ++i) {
                     const Value* incoming = ssi_phi->getIncomingValue(i);
 
                     if (incoming != V) {
                         any_value_diff_V = true;
                         break;
                     }
                 }
 
                 if (!any_value_diff_V) {
 
                     if (Verbose) {
                         errs() << "Erasing " << ssi_phi->getName() << "\n";
                     }
 
                     to_be_erased.insert(ssi_phi);
                     maptooldvalues[ssi_phi] = V;
 
                     continue;
                 }
 
                 // Second case
                 if (!this->DTmap->dominates(V, ssi_phi)) {
 
                     if (Verbose) {
                         errs() << "Erasing " << ssi_phi->getName() << "\n";
                     }
 
                     to_be_erased.insert(ssi_phi);
                     maptooldvalues[ssi_phi] = V;
 
                     continue;
                 }
 
                 if (ssi_phi->use_empty()) {
                     if (Verbose) {
                         errs() << "Erasing " << ssi_phi->getName() << "\n";
                     }
 
                     to_be_erased.insert(ssi_phi);
                     maptooldvalues[ssi_phi] = V;
 
                     continue;
                 }
             }
             // SSI_sigmas and SSI_copies have two cases for cleaning
             // First: they don't have any use
             // Second: they aren't dominated by V.
             else if (is_SSIsigma(newvar) || is_SSIcopy(newvar)) {
                 if (newvar->use_empty()) {
                     if (Verbose) {
                         errs() << "Erasing " << newvar->getName() << "\n";
                     }
                     to_be_erased.insert(newvar);
                 }
                 else if (!this->DTmap->dominates(V, newvar)) {
 
                     if (Verbose) {
                         errs() << "Erasing " << newvar->getName() << "\n";
                     }
 
                     to_be_erased.insert(newvar);
                     maptooldvalues[newvar] = V;
                 }
             }
             else {
                 errs() << "Problem here3\n";
             }
         }
     }
 
     // Create a topological sort of to be erased, based on this->versions
     // This way, we can remove instructions in a order that respects dependency
     SmallVector<Instruction*, 8> topsort = get_topsort_versions(to_be_erased);
 
     for (SmallVector<Instruction*, 8>::iterator sit = topsort.begin(), send =
             topsort.end(); sit != send; ++sit) {
         Instruction* I = *sit;
 
         DenseMap<Instruction*, Instruction*>::iterator it = maptooldvalues.find(
                 I);
 
         if (it != maptooldvalues.end()) {
             I->replaceAllUsesWith(it->second);
         }
 
         // STATISTICS
         if (is_SSIphi(I)) {
             ++NumPHIsDeleted;
         }
         else if (is_SSIsigma(I)) {
             ++NumSigmasDeleted;
         }
         else if (is_SSIcopy(I)) {
             ++NumCopiesDeleted;
         }
 
         I->eraseFromParent();
     }
 }
 
 bool SSIfy::is_SSIphi(const Instruction* I)
 {
     return I->getName().startswith(phiname);
 }
 
 bool SSIfy::is_SSIsigma(const Instruction* I)
 {
     return I->getName().startswith(signame);
 }
 
 bool SSIfy::is_SSIcopy(const Instruction* I)
 {
     return I->getName().startswith(copname);
 }
 
 SmallPtrSet<BasicBlock*, 4> SSIfy::get_iterated_df(BasicBlock* BB) const
 {
     SmallPtrSet<BasicBlock*, 4> iterated_df;
 
     SmallVector<BasicBlock*, 4> stack;
     BasicBlock* current = BB;
 
     // Initialize the stack with the original BasicBlock
     // this stack is further populated with BasicBlocks
     // in the iterated DF of the original BB, until
     // this iterated DF ends.
     stack.push_back(current);
 
     while (!stack.empty()) {
         current = stack.back();
         stack.pop_back();
 
         const DominanceFrontier::DomSetType& frontier = this->DFmap->find(
                 current)->second;
 
         for (DominanceFrontier::DomSetType::iterator fit = frontier.begin(),
                 fend = frontier.end(); fit != fend; ++fit) {
             BasicBlock* BB_infrontier = *fit;
 
             // Only push to stack if this BasicBlock wasn't seen before
             // P.S.: insert returns a pair. The second refers to whether
             // the element was actually inserted or not.
             if ((iterated_df.insert(BB_infrontier)).second) {
                 stack.push_back(BB_infrontier);
             }
         }
     }
 
     return iterated_df;
 }
 
 SmallPtrSet<BasicBlock*, 4> SSIfy::get_iterated_pdf(BasicBlock* BB) const
 {
     SmallPtrSet<BasicBlock*, 4> iterated_pdf;
 
     SmallVector<BasicBlock*, 4> stack;
     BasicBlock* current = BB;
 
     // Initialize the stack with the original BasicBlock
     // this stack is further populated with BasicBlocks
     // in the iterated PDF of the original BB, until
     // this iterated PDF ends.
     stack.push_back(current);
 
     while (!stack.empty()) {
         current = stack.back();
         stack.pop_back();
 
         const PostDominanceFrontier::DomSetType& frontier = this->PDFmap->find(
                 current)->second;
 
         for (PostDominanceFrontier::DomSetType::iterator fit = frontier.begin(),
                 fend = frontier.end(); fit != fend; ++fit) {
             BasicBlock* BB_infrontier = *fit;
 
             // Only push to stack if this BasicBlock wasn't seen before
             // P.S.: insert returns a pair. The second refers to whether
             // the element was actually inserted or not.
             if ((iterated_pdf.insert(BB_infrontier)).second) {
                 stack.push_back(BB_infrontier);
             }
         }
     }
 
     return iterated_pdf;
 }
 
 bool SSIfy::is_actual(const Instruction* I)
 {
     if (is_SSIphi(I)) {
         return false;
     }
     if (is_SSIsigma(I)) {
         return false;
     }
     if (is_SSIcopy(I)) {
         return false;
     }
 
     return true;
 }
 
 SmallVector<Instruction*, 8> SSIfy::get_topsort_versions(
         const SmallPtrSet<Instruction*, 16>& to_be_erased) const
 {
     SmallVector<Instruction*, 8> topsort;
 
     // Create a graph of precedence from Versions' keys
     Graph g;
 
     // Add nodes
     for (SmallPtrSetIterator<Instruction*> sit = to_be_erased.begin(), send =
             to_be_erased.end(); sit != send; ++sit) {
         g.addNode(*sit);
     }
 
     // Add edges
     for (DenseMap<Value*, SmallPtrSet<Instruction*, 4> >::const_iterator mit =
             versions.begin(), mend = versions.end(); mit != mend; ++mit) {
         Value* V = mit->first;
         const SmallPtrSet<Instruction*, 4>& set = mit->second;
 
         if (!g.hasNode(V)) {
             continue;
         }
 
         for (SmallPtrSetIterator<Instruction*> sit = set.begin(), send =
                 set.end(); sit != send; ++sit) {
             g.addEdge(V, *sit);
         }
     }
 
     // Let's start, shall we
     SmallPtrSet<Value*, 8> unmarked_nodes(to_be_erased.begin(),
             to_be_erased.end());
 
     while (!unmarked_nodes.empty()) {
         SmallPtrSetIterator<Value*> sit = unmarked_nodes.begin();
         visit(g, unmarked_nodes, topsort, *sit);
     }
 
     // topsort now contains a topological sorting of nodes
     return topsort;
 }
 
 void SSIfy::visit(Graph& g, SmallPtrSet<Value*, 8>& unmarked_nodes,
         SmallVectorImpl<Instruction*>& list, Value* V) const
 {
     if (unmarked_nodes.count(V)) {
         const SmallPtrSet<Value*, 4>& adj_list = g.vertices[V];
 
         for (SmallPtrSetIterator<Value*> sit = adj_list.begin(), send =
                 adj_list.end(); sit != send; ++sit) {
             Value* m = *sit;
             visit(g, unmarked_nodes, list, m);
         }
 
         unmarked_nodes.erase(V);
 
         list.push_back(cast<Instruction>(V));
     }
 }
 
 bool SSIfy::isNotNecessary(const Instruction* insert_point,
         const Value* V) const
 {
     for (Value::const_user_iterator uit = V->user_begin(), uend = V->user_end();
             uit != uend; ++uit) {
         const User *U = *uit;
         const Instruction* use = cast<Instruction>(U);
 
         if (this->DTmap->dominates(insert_point, use)) {
             return false;
         }
     }
 
     return true;
 }
