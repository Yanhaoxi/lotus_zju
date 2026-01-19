/**
 * @file StaticVectorClockMHP.cpp
 * @brief Implementation of the static vector clock based MHP analysis.
 */

#include "Analysis/Concurrency/StaticVectorClockMHP.h"

#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/raw_ostream.h>

#include <deque>
#include <unordered_set>

using namespace llvm;
using namespace mhp;

StaticVectorClockMHP::StaticVectorClockMHP(Module &module)
    : m_module(module), m_thread_api(ThreadAPI::getThreadAPI()) {
  m_tfg = std::make_unique<ThreadFlowGraph>();
}

void StaticVectorClockMHP::analyze() {
  // Build a fresh thread-flow graph locally (no dependency on MHPAnalysis).
  buildThreadFlowGraph();

  buildStaticThreads();
  computeStaticVectorClocks();
  computeMHPPairs();
}

// === StaticVectorClock helpers =============================================

bool StaticVectorClockMHP::StaticVectorClock::mergeFrom(
    const StaticVectorClock &other) {
  bool changed = false;
  for (const auto &kv : other.entries) {
    auto &dest = entries[kv.first];
    for (const auto &elem : kv.second) {
      changed |= dest.insert(elem).second;
    }
  }
  return changed;
}

bool StaticVectorClockMHP::StaticVectorClock::leq(
    const StaticVectorClock &other) const {
  for (const auto &kv : entries) {
    auto it = other.entries.find(kv.first);
    if (it == other.entries.end()) {
      if (!kv.second.empty())
        return false;
      continue;
    }
    const auto &rhs = it->second;
    for (const auto &elem : kv.second) {
      if (rhs.find(elem) == rhs.end())
        return false;
    }
  }
  return true;
}

StaticVectorClockMHP::StaticVectorClock
StaticVectorClockMHP::initialClockFor(const StaticThread &st) const {
  StaticVectorClock init;
  LogicClockSet starter;
  starter.insert({LogicClockElem::Kind::Start, 0});
  init.entries[st.id] = std::move(starter);
  return init;
}

StaticThreadID StaticVectorClockMHP::getOrCreateStaticThread(const Context &ctx,
                                                             ThreadID base_tid,
                                                             const SyncNode *entry) {
  auto it = m_ctx_to_stid.find(ctx);
  if (it != m_ctx_to_stid.end())
    return it->second;

  StaticThreadID new_id = m_static_threads.size();
  StaticThread st;
  st.id = new_id;
  st.ctx = ctx;
  st.base_tid = base_tid;
  st.entry = entry;
  m_static_threads.push_back(st);
  m_ctx_to_stid[ctx] = new_id;
  return new_id;
}

void StaticVectorClockMHP::buildStaticThreads() {
  m_static_threads.clear();
  m_ctx_to_stid.clear();
  m_node_to_static_thread.clear();
  m_inst_to_static_thread.clear();
  m_node_clocks.clear();

  if (!m_tfg)
    return;

  const SyncNode *main_entry = m_tfg->getThreadEntryNode(0);
  Context root;
  StaticThreadID root_id = getOrCreateStaticThread(root, 0, main_entry);

  std::deque<StaticThreadID> worklist;
  worklist.push_back(root_id);

  while (!worklist.empty()) {
    StaticThreadID stid = worklist.front();
    worklist.pop_front();

    StaticThread &st = m_static_threads[stid];
    if (!st.entry)
      continue;

    std::unordered_set<const SyncNode *> visited;
    std::deque<const SyncNode *> nq;
    nq.push_back(st.entry);
    visited.insert(st.entry);

    while (!nq.empty()) {
      const SyncNode *node = nq.front();
      nq.pop_front();

      st.nodes.push_back(node);
      m_node_to_static_thread[node] = stid;
      if (const Instruction *inst = node->getInstruction()) {
        m_inst_to_static_thread[inst] = stid;
      }
      if (m_node_clocks.find(node) == m_node_clocks.end()) {
        m_node_clocks.insert(std::make_pair(node, initialClockFor(st)));
      }

      // Handle fork to spawn new static thread context
      if (node->getType() == SyncNodeType::THREAD_FORK) {
        ThreadID child_tid = node->getForkedThread();
        const SyncNode *child_entry = m_tfg->getThreadEntryNode(child_tid);
        if (child_entry) {
          Context child_ctx = st.ctx;
          child_ctx.fork_sites.push_back(node->getNodeID());
          StaticThreadID child_stid =
              getOrCreateStaticThread(child_ctx, child_tid, child_entry);
          // Only enqueue if first time we saw it
          if (m_static_threads[child_stid].nodes.empty()) {
            worklist.push_back(child_stid);
          }
        }
      }

      // Traverse successors in the same base thread to build the intra-thread CFG
      for (auto *succ : node->getSuccessors()) {
        if (succ->getThreadID() != st.base_tid)
          continue;
        if (visited.insert(succ).second) {
          nq.push_back(succ);
        }
      }
    }
  }
}

StaticVectorClockMHP::StaticVectorClock
StaticVectorClockMHP::mergePredecessorClocks(const SyncNode *node) const {
  StaticVectorClock merged;
  if (!node)
    return merged;

  for (auto *pred : node->getPredecessors()) {
    auto it = m_node_clocks.find(pred);
    if (it != m_node_clocks.end()) {
      merged.mergeFrom(it->second);
    }
  }
  return merged;
}

void StaticVectorClockMHP::addEventToClock(const SyncNode *node,
                                           StaticVectorClock &sv) const {
  auto st_it = m_node_to_static_thread.find(node);
  if (st_it == m_node_to_static_thread.end())
    return;

  StaticThreadID stid = st_it->second;
  LogicClockElem elem{LogicClockElem::Kind::Node, node->getNodeID()};
  sv.entries[stid].insert(elem);

  if (node->getType() == SyncNodeType::THREAD_EXIT) {
    sv.entries[stid].insert({LogicClockElem::Kind::Terminated, 0});
  }
}

bool StaticVectorClockMHP::transfer(const SyncNode *node) {
  if (!node)
    return false;

  // Vector Clock Update Rule:
  // 1. Merge (Join) vector clocks from all predecessors (max per thread component).
  //    VC_in(n) = max(VC_out(p)) for all p in preds(n)
  // 2. Increment the local thread's clock component for the current event.
  //    VC_out(n) = VC_in(n); VC_out(n)[tid]++
  // 3. Update the node's clock. Return true if changed (for fixpoint).

  StaticVectorClock incoming = mergePredecessorClocks(node);

  // If no predecessors, seed with the static thread's initial clock.
  if (incoming.entries.empty()) {
    auto st_it = m_node_to_static_thread.find(node);
    if (st_it != m_node_to_static_thread.end()) {
      incoming = initialClockFor(m_static_threads[st_it->second]);
    }
  }

  addEventToClock(node, incoming);

  StaticVectorClock &current = m_node_clocks[node];
  bool changed = !incoming.leq(current) || !current.leq(incoming);
  if (changed) {
    current = incoming;
  }
  return changed;
}

void StaticVectorClockMHP::computeStaticVectorClocks() {
  if (!m_tfg)
    return;

  bool changed = true;
  while (changed) {
    changed = false;
    for (const auto &pair : m_node_clocks) {
      const SyncNode *node = pair.first;
      changed |= transfer(node);
    }
  }
}

bool StaticVectorClockMHP::happensBefore(const StaticVectorClock &lhs,
                                         const StaticVectorClock &rhs) const {
  // Returns true if lhs happens-before rhs (strictly ordered)
  // Logic: lhs <= rhs AND !(rhs <= lhs)
  return lhs.leq(rhs) && !rhs.leq(lhs);
}

bool StaticVectorClockMHP::happensBefore(const Instruction *i1,
                                         const Instruction *i2) const {
  if (!m_tfg)
    return false;

  if (i1 == i2)
    return false;

  const SyncNode *n1 = m_tfg->getNode(i1);
  const SyncNode *n2 = m_tfg->getNode(i2);
  if (!n1 || !n2)
    return false;

  auto c1_it = m_node_clocks.find(n1);
  auto c2_it = m_node_clocks.find(n2);
  if (c1_it == m_node_clocks.end() || c2_it == m_node_clocks.end())
    return false;

  return happensBefore(c1_it->second, c2_it->second);
}

void StaticVectorClockMHP::computeMHPPairs() {
  std::vector<const Instruction *> all_insts;
  all_insts.reserve(m_inst_to_static_thread.size());
  for (Function &func : m_module) {
    for (inst_iterator I = inst_begin(func), E = inst_end(func); I != E; ++I) {
      const Instruction *inst = &*I;
      if (m_inst_to_static_thread.count(inst)) {
        all_insts.push_back(inst);
      }
    }
  }

  for (size_t i = 0; i < all_insts.size(); ++i) {
    for (size_t j = i + 1; j < all_insts.size(); ++j) {
      const Instruction *a = all_insts[i];
      const Instruction *b = all_insts[j];

      if (happensBefore(a, b) || happensBefore(b, a)) {
        continue;
      }

      m_mhp_pairs.insert({a, b});
    }
  }
}

bool StaticVectorClockMHP::mayHappenInParallel(const Instruction *i1,
                                               const Instruction *i2) const {
  if (!i1 || !i2 || i1 == i2)
    return false;

  if (m_mhp_pairs.count({i1, i2}) || m_mhp_pairs.count({i2, i1}))
    return true;

  return !happensBefore(i1, i2) && !happensBefore(i2, i1);
}

void StaticVectorClockMHP::printStatistics(raw_ostream &os) const {
  size_t num_static_threads = m_static_threads.size();
  size_t num_nodes = m_node_clocks.size();

  os << "SVC-MHP Statistics:\n";
  os << "===================\n";
  os << "Static Threads: " << num_static_threads << "\n";
  os << "TFG Nodes:      " << num_nodes << "\n";
  os << "MHP Pairs:      " << m_mhp_pairs.size() << "\n";
}

void StaticVectorClockMHP::printResults(raw_ostream &os) const {
  printStatistics(os);
  os << "\nSample MHP pairs (up to 10):\n";
  int shown = 0;
  for (const auto &p : m_mhp_pairs) {
    os << "MHP: ";
    p.first->print(os);
    os << " || ";
    p.second->print(os);
    os << "\n";
    if (++shown >= 10) {
      if (m_mhp_pairs.size() > 10) {
        os << "... (" << (m_mhp_pairs.size() - 10) << " more)\n";
      }
      break;
    }
  }
}

// === Thread-flow graph construction (self contained, no MHPAnalysis) ========

void StaticVectorClockMHP::buildThreadFlowGraph() {
  m_tfg = std::make_unique<ThreadFlowGraph>();

  Function *main_func = m_module.getFunction("main");
  if (!main_func) {
    errs() << "SVC-MHP: no main function found\n";
    return;
  }

  m_tfg->addThread(0, main_func);
  processFunction(main_func, 0);
}

void StaticVectorClockMHP::processFunction(const Function *func, ThreadID tid) {
  if (!func || func->isDeclaration())
    return;

  auto &visited = m_visited_functions_by_thread[tid];
  if (!visited.insert(func).second)
    return;

  SyncNode *prev_node = nullptr;
  SyncNode *entry_node = nullptr;

  if (const SyncNode *existing = m_tfg->getThreadEntryNode(tid)) {
    entry_node = const_cast<SyncNode *>(existing);
    prev_node = entry_node;
  }

  for (const BasicBlock &bb : *func) {
    for (const Instruction &inst : bb) {
      mapInstructionToThread(&inst, tid);

      SyncNode *node = nullptr;

      if (const CallBase *cb = dyn_cast<CallBase>(&inst)) {
        if (m_thread_api->isTDFork(&inst)) {
          node = m_tfg->createNode(&inst, SyncNodeType::THREAD_FORK, tid);
          handleThreadFork(&inst, node);
        } else if (m_thread_api->isTDJoin(&inst)) {
          node = m_tfg->createNode(&inst, SyncNodeType::THREAD_JOIN, tid);
          handleThreadJoin(&inst, node);
        } else if (m_thread_api->isTDAcquire(&inst)) {
          node = m_tfg->createNode(&inst, SyncNodeType::LOCK_ACQUIRE, tid);
          handleLockAcquire(&inst, node);
        } else if (m_thread_api->isTDRelease(&inst)) {
          node = m_tfg->createNode(&inst, SyncNodeType::LOCK_RELEASE, tid);
          handleLockRelease(&inst, node);
        } else if (m_thread_api->isTDExit(&inst)) {
          node = m_tfg->createNode(&inst, SyncNodeType::THREAD_EXIT, tid);
        } else if (m_thread_api->isTDCondWait(&inst)) {
          node = m_tfg->createNode(&inst, SyncNodeType::COND_WAIT, tid);
          handleCondWait(&inst, node);
        } else if (m_thread_api->isTDCondSignal(&inst)) {
          node = m_tfg->createNode(&inst, SyncNodeType::COND_SIGNAL, tid);
          handleCondSignal(&inst, node);
        } else if (m_thread_api->isTDCondBroadcast(&inst)) {
          node = m_tfg->createNode(&inst, SyncNodeType::COND_BROADCAST, tid);
          handleCondSignal(&inst, node);
        } else if (m_thread_api->isTDBarWait(&inst)) {
          node = m_tfg->createNode(&inst, SyncNodeType::BARRIER_WAIT, tid);
          handleBarrier(&inst, node);
        } else {
          // Non-thread API call: process callee in same thread
          const Function *callee = cb->getCalledFunction();
          if (callee && !callee->isDeclaration()) {
            processFunction(callee, tid);
          }
        }
      }

      if (!node) {
        node = m_tfg->createNode(&inst, SyncNodeType::REGULAR_INST, tid);
      }

      if (prev_node) {
        m_tfg->addIntraThreadEdge(prev_node, node);
      }

      if (!entry_node) {
        entry_node = node;
        m_tfg->setThreadEntryNode(tid, entry_node);
      }

      prev_node = node;
    }
  }

  if (prev_node && !m_tfg->getThreadExitNode(tid)) {
    m_tfg->setThreadExitNode(tid, prev_node);
  }
}

void StaticVectorClockMHP::mapInstructionToThread(const Instruction *inst,
                                                  ThreadID tid) {
  m_inst_to_thread[inst] = tid;
}

ThreadID StaticVectorClockMHP::allocateThreadID() { return m_next_thread_id++; }

void StaticVectorClockMHP::handleThreadFork(const Instruction *fork_inst,
                                            SyncNode *node) {
  ThreadID new_tid = allocateThreadID();
  ThreadID parent_tid = m_inst_to_thread[fork_inst];

  node->setForkedThread(new_tid);

  m_thread_fork_sites[new_tid] = fork_inst;
  m_thread_parents[new_tid] = parent_tid;
  m_thread_children[parent_tid].push_back(new_tid);

  const Value *pthread_ptr = m_thread_api->getForkedThread(fork_inst);
  if (pthread_ptr) {
    m_pthread_value_to_thread[pthread_ptr] = new_tid;
    m_thread_to_pthread_value[new_tid] = pthread_ptr;
  }

  const Value *forked_fun_val = m_thread_api->getForkedFun(fork_inst);
  if (const Function *forked_fun = dyn_cast_or_null<Function>(forked_fun_val)) {
    m_tfg->addThread(new_tid, forked_fun);
    processFunction(forked_fun, new_tid);
    if (SyncNode *child_entry = m_tfg->getThreadEntryNode(new_tid)) {
      m_tfg->addInterThreadEdge(node, child_entry);
    }
  }
}

void StaticVectorClockMHP::handleThreadJoin(const Instruction *join_inst,
                                            SyncNode *node) {
  const Value *joined_thread_val = m_thread_api->getJoinedThread(join_inst);
  ThreadID joined_tid = 0;
  bool found = false;

  if (joined_thread_val) {
    auto it = m_pthread_value_to_thread.find(joined_thread_val);
    if (it != m_pthread_value_to_thread.end()) {
      joined_tid = it->second;
      found = true;
    } else if (const LoadInst *load = dyn_cast<LoadInst>(joined_thread_val)) {
      const Value *loaded_from = load->getPointerOperand();
      auto it2 = m_pthread_value_to_thread.find(loaded_from);
      if (it2 != m_pthread_value_to_thread.end()) {
        joined_tid = it2->second;
        found = true;
        m_pthread_value_to_thread[joined_thread_val] = joined_tid;
      }
    }
  }

  auto add_edge_to_join = [&](ThreadID tid) {
    if (SyncNode *child_exit = m_tfg->getThreadExitNode(tid)) {
      m_tfg->addInterThreadEdge(child_exit, node);
      node->setJoinedThread(tid);
      m_join_to_thread[join_inst] = tid;
    }
  };

  if (found && joined_tid != 0) {
    add_edge_to_join(joined_tid);
  } else {
    ThreadID parent_tid = m_inst_to_thread[join_inst];
    if (m_thread_children.count(parent_tid)) {
      for (ThreadID child_tid : m_thread_children[parent_tid]) {
        add_edge_to_join(child_tid);
      }
    }
  }
}

void StaticVectorClockMHP::handleLockAcquire(const Instruction *lock_inst,
                                             SyncNode *node) {
  const Value *lock = m_thread_api->getLockVal(lock_inst);
  node->setLockValue(lock);
}

void StaticVectorClockMHP::handleLockRelease(const Instruction *unlock_inst,
                                             SyncNode *node) {
  const Value *lock = m_thread_api->getLockVal(unlock_inst);
  node->setLockValue(lock);
}

void StaticVectorClockMHP::handleCondWait(const Instruction *wait_inst,
                                          SyncNode *node) {
  const Value *cond = m_thread_api->getCondVal(wait_inst);
  const Value *mutex = m_thread_api->getCondMutex(wait_inst);
  node->setCondValue(cond);
  node->setLockValue(mutex);
  m_condvar_waits[cond].push_back(wait_inst);
}

void StaticVectorClockMHP::handleCondSignal(const Instruction *signal_inst,
                                            SyncNode *node) {
  const Value *cond = m_thread_api->getCondVal(signal_inst);
  node->setCondValue(cond);
  m_condvar_signals[cond].push_back(signal_inst);
}

void StaticVectorClockMHP::handleBarrier(const Instruction *barrier_inst,
                                         SyncNode *node) {
  const Value *barrier = m_thread_api->getBarrierVal(barrier_inst);
  node->setLockValue(barrier);
  m_barrier_waits[barrier].push_back(barrier_inst);
}
