#include "Verification/SymbolicAbstraction/Core/Fragment.h"
#include "Verification/SymbolicAbstraction/Core/repr.h"

#include <llvm/IR/CFG.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Instructions.h>

#include <queue>

namespace symbolic_abstraction
{
using namespace llvm;

llvm::BasicBlock* Fragment::EXIT = nullptr;

namespace
{
// recursive DFS to find loops. EXIT node can be ignored as it cannot be
// contained in a loop.
bool findLoop(std::set<Fragment::edge>& edges,
              std::unordered_set<llvm::BasicBlock*>& seen, llvm::BasicBlock* bb)
{
    if (seen.find(bb) != seen.end()) { // we found a bb that we have seen before
        seen.clear();
        return true;
    }

    seen.insert(bb);

    for (auto itr_bb_to = succ_begin(bb), end = succ_end(bb); itr_bb_to != end;
         ++itr_bb_to) {
        if (edges.find(std::make_pair(bb, *itr_bb_to)) != edges.end()) {
            // allowed edge
            if (findLoop(edges, seen, *itr_bb_to)) {
                return true;
            }
        }
    }

    seen.erase(bb);

    return false;
}
} // namespace

/**
 * Decide for a set of edges and a set of starting nodes whether any of
 * the starting nodes has a path to a loop (only using the given edges)
 *
 * \returns   true iff there exists a loop.
 */
bool Fragment::hasLoops()
{
    std::unordered_set<llvm::BasicBlock*> seen;
    return findLoop(Edges_, seen, Start_);
}

std::ostream& operator<<(std::ostream& out, const Fragment& frag)
{
    assert(frag.getStart() != nullptr);
    out << repr(frag.getStart()) << "-->";

    if (frag.getEnd() == Fragment::EXIT)
        out << "EXIT";
    else
        out << repr(frag.getEnd());

    if (frag.includesEndBody())
        out << "+";

    return out;
}

std::vector<Fragment::edge> Fragment::edgesFrom(BasicBlock* location) const
{
    assert(locations().find(location) != locations().end());
    std::vector<edge> result;

    if (location == Fragment::EXIT)
        return result;

    auto itr = succ_begin(location), end = succ_end(location);

    if (itr == end) {
        if (edges().find({location, EXIT}) != edges().end()) {
            result.push_back({location, EXIT});
        }
    }

    for (; itr != end; ++itr) {
        if (edges().find({location, *itr}) != edges().end()) {
            result.push_back({location, *itr});
        }
    }

    return result;
}

std::vector<Fragment::edge> Fragment::edgesTo(BasicBlock* location) const
{
    assert(locations().find(location) != locations().end());
    std::vector<edge> result;

    for (edge e : edges()) {
        if (e.second == location)
            result.push_back(e);
    }

    return result;
}

bool Fragment::isPredecessor(const Fragment& frag) const
{
    return getStart() == frag.getEnd();
}

bool Fragment::defines(llvm::Value* value) const
{
    auto *inst = dyn_cast<llvm::Instruction>(value);

    if (inst == nullptr)
        return false;

    llvm::BasicBlock* bb = inst->getParent();

    if (llvm::isa<llvm::PHINode>(inst)) {
        // A phi is defined on the edge prev_block->bb. Check whether at
        // least one such edge is in this fragment.
        for (auto itr = llvm::pred_begin(bb), end = llvm::pred_end(bb);
             itr != end; ++itr) {

            if (Edges_.find({*itr, bb}) != Edges_.end())
                return true;
        }
        return false;
    } else {
        // If this fragment includes the whole ending BB, it defines everything
        // in it.
        if (includesEndBody() && inst->getParent() == getEnd())
            return true;

        // Non-phi instructions are defined on the edge from bb.
        auto itr = llvm::succ_begin(bb), end = llvm::succ_end(bb);

        if (itr == end) {
            // check for a "virtual" edge bb->EXIT
            if (Edges_.find({bb, Fragment::EXIT}) != Edges_.end())
                return true;
        } else {
            for (; itr != end; ++itr) {
                if (Edges_.find({bb, *itr}) != Edges_.end())
                    return true;
            }
        }
        return false;
    }
}

bool Fragment::reachable(Instruction* a, Instruction* b) const
{
    assert(defines(a));

    // handle as a special case if a and b are in the same block
    if (a->getParent() == b->getParent()) {
        std::vector<Instruction*> insts;

        if (getStart() == getEnd() && a->getParent() == getStart()) {
            // This block is both a starting and ending block of this fragment.
            // In this case, the phi instructions actually come *after* the
            // non phis.
            for (auto& x : *getStart()) {
                if (!isa<PHINode>(x))
                    insts.push_back(&x);
            }

            for (auto& x : *getStart()) {
                if (isa<PHINode>(x))
                    insts.push_back(&x);
            }
        } else {
            for (auto& x : a->getParent()->getInstList())
                insts.push_back(&x);
        }

        auto a_itr = std::find(insts.begin(), insts.end(), a);
        auto b_itr = std::find(insts.begin(), insts.end(), b);
        return a_itr <= b_itr;
    }

    if (!defines(b))
        return false;

    if (Start_ == b->getParent() && !isa<PHINode>(b)) {
        // If b is in Start_ and it is no PHINode, it can only be reachable
        // from a if a is also in Start_, which would have been handled before.
        return false;
    }
    // Perform a BFS from a. Since fragments are acyclic, it's not necessary to
    // keep track of visited locations.
    std::queue<BasicBlock*> queue;
    queue.push(a->getParent());

    bool first_time_start = true;
    while (!queue.empty()) {
        BasicBlock* bb = queue.front();
        queue.pop();

        if (bb == b->getParent()) {
            // If b is a phi, it's reachable as long as bb is reachable. If b is
            // not a phi, there must be at least one outgoing edge from bb since
            // this->defines(b).
            return true;
        }

        bool edge_from_end_necessary = (Start_ == End_) &&
                                       (a->getParent() == Start_) &&
                                       !isa<PHINode>(a) && first_time_start;
        // Since Fragments have to be (almost) acyclic, the only possibility of
        // a transition from End_ in the Fragment is that End_ == Start_.
        // Adding the destination of such a transition to the queue here is
        // only desirable if
        //  - the start instruction a of the BFS lies in Start_
        //  - it is not a PHINode
        // (this means that the non-phi part of the block is actually needed).
        // It is necessary to assure that such successors are added exactly
        // once as otherwise, with Start_ == End_, the BFS might diverge.
        if (End_ != bb || edge_from_end_necessary) {
            // Note that we don't add Fragment::EXIT to the queue since it's not
            // possible for b->getParent() to be equal to Fragment::EXIT
            for (auto itr = succ_begin(bb), end = succ_end(bb); itr != end;
                 ++itr) {
                if (Edges_.find({bb, *itr}) != Edges_.end())
                    queue.push(*itr);
            }
            // if the above mentioned case (with Start_ == End_ and a really in
            // Start_) occurs, the successors of Start_ are added in the first
            // iteration, therefore setting the flag to false here is correct.
            first_time_start = false;
        }
    }

    return false;
}
} // namespace symbolic_abstraction
