/// @file InterProceduralPass.cpp
/// @brief LLVM module pass implementing inter-procedural pointer analysis with call graph construction
///
/// This file implements `LotusAA`, the top-level LLVM ModulePass that orchestrates
/// **whole-program pointer analysis** and **on-the-fly call graph construction**.
///
/// **Pass Architecture:**
/// ```
/// LotusAA::runOnModule(Module)
///   ├── Initialize global structures (NullObj, UnknownObj, sentinel values)
///   ├── computeGlobalHeuristic() - Analyze global initializers
///   ├── computePtsCgIteratively() - Main fixpoint algorithm
///   │   ├── initFuncProcessingSeq() - Build call graph, topological sort
///   │   ├── For each function (bottom-up):
///   │   │   └── computePTA(func) - Run intra-procedural analysis
///   │   ├── computeCG() - Resolve indirect calls
///   │   ├── Detect changes, iterate until fixpoint
///   │   └── detectBackEdges() - Handle recursion
///   └── finalizeCg() - Print results (if enabled)
/// ```
///
/// **On-the-Fly Call Graph Construction:**
/// The analysis alternates between pointer analysis and call graph refinement:
/// 1. Analyze functions bottom-up using current call graph
/// 2. Resolve indirect calls using pointer analysis results
/// 3. Update call graph with newly discovered edges
/// 4. Reanalyze affected functions
/// 5. Repeat until fixpoint (no new edges discovered)
///
/// **Fixpoint Iteration:**
/// ```
/// iter = 0
/// changed = all_functions
/// while changed and iter < max_iter:
///   for func in bottom_up_order:
///     if func in changed:
///       interface_changed = analyze(func)
///       if interface_changed:
///         changed += callers_of(func)
///   update_call_graph_from_FP_results()
///   detect_back_edges()
///   iter++
/// ```
///
/// **Key Features:**
/// - **Context-Sensitive**: Function summaries provide calling context
/// - **Flow-Sensitive**: SSA-based intra-procedural analysis
/// - **On-the-Fly CG**: No pre-computed call graph needed
/// - **Scalable**: Iterates only on changed functions
///
/// **Command-line Options:**
/// - `--lotus-cg`: Enable call graph construction (default: on)
/// - `--lotus-restrict-cg-iter`: Max CG iterations (default: 5)
/// - `--lotus-print-pts`: Print points-to results
/// - `--lotus-print-cg`: Print resolved call graph
/// - `--lotus-enable-global-heuristic`: Analyze global initializers
///
/// **Registered Pass:**
/// - Pass ID: "lotus-aa"
/// - Description: "LotusAA: Flow- and context-sensitive alias analysis"
///
/// @see IntraLotusAA for per-function analysis
/// @see CallGraphState for call graph management
/// @see FunctionPointerResults for storing resolved indirect calls

#include "Alias/LotusAA/Engine/InterProceduralPass.h"
#include "Alias/LotusAA/Engine/IntraProceduralAnalysis.h"
#include "Alias/LotusAA/MemoryModel/PointsToGraph.h"
#include "Alias/LotusAA/MemoryModel/MemObject.h"
#include "Alias/LotusAA/Support/LotusConfig.h"

#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Dominators.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/ThreadPool.h>
#include <llvm/Support/Threading.h>
#include <llvm/Support/raw_ostream.h>

#include <algorithm>
#include <future>

using namespace llvm;
using namespace std;

// Command-line options
static cl::opt<bool> lotus_cg(
    "lotus-cg",
    cl::desc("Use LotusAA to build call graph"),
    cl::init(LotusConfig::DebugOptions::DEFAULT_ENABLE_CG));

static cl::opt<int> lotus_restrict_cg_iter(
    "lotus-restrict-cg-iter",
    cl::desc("Maximum iterations for call graph construction"),
    cl::init(LotusConfig::CallGraphLimits::DEFAULT_MAX_ITERATIONS));

static cl::opt<bool> lotus_enable_global_heuristic(
    "lotus-enable-global-heuristic",
    cl::desc("Enable heuristic for global pointer handling"),
    cl::init(LotusConfig::Heuristics::DEFAULT_ENABLE_GLOBAL_HEURISTIC));

static cl::opt<bool> lotus_print_pts(
    "lotus-print-pts",
    cl::desc("Print LotusAA points-to results"),
    cl::init(LotusConfig::DebugOptions::DEFAULT_PRINT_PTS));

static cl::opt<bool> lotus_print_cg(
    "lotus-print-cg",
    cl::desc("Print LotusAA call graph results"),
    cl::init(LotusConfig::DebugOptions::DEFAULT_PRINT_CG));

static cl::opt<unsigned> lotus_parallel_threads(
    "lotus-aa-threads",
    cl::desc("Number of threads for LotusAA (0 = auto)"),
    cl::init(0));

char LotusAA::ID = 0;
static RegisterPass<LotusAA> X("lotus-aa",
                                "LotusAA: Flow-sensitive alias analysis",
                                false, /* CFG only */
                                true   /* is analysis */);

LotusAA::LotusAA() : ModulePass(ID), DL(nullptr) {}

LotusAA::~LotusAA() {
  delete MemObject::NullObj;
  delete MemObject::UnknownObj;
  
  // Note: Don't delete Arguments - they're LLVM-managed
  // The sentinel values (FREE_VARIABLE, etc.) are Arguments but
  // we created them, so we shouldn't delete them either

  for (auto &func_result : intraResults_) {
    if (func_result.second)
      delete func_result.second;
  }

  // Clean up cached dominator trees
  for (auto &dt_pair : dominatorTrees_) {
    if (dt_pair.second)
      delete dt_pair.second;
  }
}

void LotusAA::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  AU.addRequired<DominatorTreeWrapperPass>();
  // Iterated dominance frontier computed on-the-fly via IDFCalculator
}

bool LotusAA::runOnModule(Module &M) {
  DL = &M.getDataLayout();

  IntraLotusAAConfig::setParam();

  // Initialize global singletons
  MemObject::NullObj = new MemObject(nullptr, nullptr, MemObject::CONCRETE);
  MemObject::NullObj->findLocator(0, true);

  MemObject::UnknownObj = new MemObject(nullptr, nullptr, MemObject::CONCRETE);
  MemObject::UnknownObj->findLocator(0, true);

  LocValue::FREE_VARIABLE = new Argument(Type::getVoidTy(M.getContext()));
  LocValue::NO_VALUE = new Argument(Type::getVoidTy(M.getContext()));
  LocValue::UNDEF_VALUE = new Argument(Type::getVoidTy(M.getContext()));
  LocValue::SUMMARY_VALUE = new Argument(Type::getVoidTy(M.getContext()));

  PTGraph::DEFAULT_NON_POINTER_TYPE = Type::getInt64Ty(M.getContext());
  PTGraph::DEFAULT_POINTER_TYPE = Type::getInt8PtrTy(M.getContext());

  // Initialize results map
  for (Function &F : M) {
    intraResults_[&F] = nullptr;
  }

  // Compute global heuristics
  if (lotus_enable_global_heuristic) {
    computeGlobalHeuristic(M);
  }

  // Compute PTS and CG iteratively
  std::vector<Function *> func_seq;
  computePtsCgIteratively(M, func_seq);

  // Finalize
  finalizeCg(func_seq);

  return false;
}

void LotusAA::computeGlobalHeuristic(Module &M) {
  for (Function &f : M) {
    for (BasicBlock &bb : f) {
      for (Instruction &inst : bb) {
        if (StoreInst *store = dyn_cast<StoreInst>(&inst)) {
          Value *ptr = store->getPointerOperand();
          Value *val = store->getValueOperand();
          if (isa<GlobalValue>(ptr) && isa<Constant>(val)) {
            globalValuesCache_[ptr].insert(val);
          }
        }
      }
    }
  }
}

void LotusAA::initFuncProcessingSeq(Module &M, std::vector<Function *> &func_seq) {
  // Simple bottom-up ordering (reverse call graph)
  map<Function *, int> out_degrees;
  
  // Clear and initialize call graph
  callGraphState_.clear();
  
  std::vector<Function *> allFunctions;
  for (Function &F : M) {
    allFunctions.push_back(&F);
    out_degrees[&F] = 0;
  }
  callGraphState_.initializeForFunctions(allFunctions);

  // Build call graph from function pointer results
  const auto &fpResults = functionPointerResults_.getResultsMap();
  for (const auto &callerResults : fpResults) {
    Function *caller = callerResults.first;
    for (const auto &callsiteResults : callerResults.second) {
      for (Function *callee : callsiteResults.second) {
        if (!callee)
          continue;

        if (!callGraphState_.isBackEdge(caller, callee)) {
          callGraphState_.addEdge(caller, callee);
          out_degrees[callee]++;
        }
      }
    }
  }

  // Topological sort
  std::vector<Function *> worklist;
  for (auto &pair : out_degrees) {
    if (pair.second == 0)
      worklist.push_back(pair.first);
  }

  func_seq.clear();
  while (!worklist.empty()) {
    Function *F = worklist.back();
    worklist.pop_back();
    
    if (F)
      func_seq.push_back(F);

    for (Function *callee : callGraphState_.getCallees(F)) {
      if (--out_degrees[callee] == 0)
        worklist.push_back(callee);
    }
  }
}

void LotusAA::initCGBackedge() {
  // Initialize from existing call graph (direct calls)
  // Scan all functions to find direct call sites
  for (auto &func_result : intraResults_) {
    Function *F = func_result.first;
    for (BasicBlock &BB : *F) {
      for (Instruction &I : BB) {
        if (CallBase *call = dyn_cast<CallBase>(&I)) {
          if (Function *callee = call->getCalledFunction()) {
            functionPointerResults_.addTarget(F, call, callee);
          }
        }
      }
    }
  }
}

void LotusAA::computePtsCgIteratively(Module &M, std::vector<Function *> &func_seq) {
  initCGBackedge();

  bool changed = true;
  int iteration = 0;
  set<Function *> changed_func;

  // Initialize: analyze all functions
  for (Function &F : M) {
    changed_func.insert(&F);
  }

  ThreadPoolStrategy strategy =
      hardware_concurrency(lotus_parallel_threads.getValue());
  const unsigned workerCount =
      std::max(1u, strategy.compute_thread_count());

  llvm::ThreadPool threadPool(strategy);
  const unsigned poolMax = std::max(1u, threadPool.getThreadCount());

  while (changed && iteration < lotus_restrict_cg_iter) {
    outs() << "[LotusAA] Iteration " << (iteration + 1)
           << " using " << poolMax << " thread(s)\n";
    if (poolMax == 1 && lotus_parallel_threads.getValue() > 1) {
      outs() << "[LotusAA] Requested " << lotus_parallel_threads.getValue()
             << " threads, but only 1 is available (LLVM threads disabled or "
             << "hardware_concurrency limited).\n";
    }

    initFuncProcessingSeq(M, func_seq);
    changed = false;

    // Build dependency counts (caller depends on its callees)
    std::map<Function *, unsigned, llvm_cmp> pendingCallees;
    for (Function *func : func_seq) {
      unsigned deps = 0;
      for (Function *callee : callGraphState_.getCallees(func)) {
        if (!callGraphState_.isBackEdge(func, callee))
          deps++;
      }
      pendingCallees[func] = deps;
    }

  std::mutex depsMutex;
    std::vector<Function *> ready;
    for (auto &item : pendingCallees) {
      if (item.second == 0)
        ready.push_back(item.first);
    }

    struct AnalysisResult {
      Function *Func;
      IntraLotusAA *OldResult;
      IntraLotusAA *NewResult;
      bool InterfaceChanged;
      bool Skipped;
    };

    std::mutex queueMutex;
    std::mutex resultsMutex;
    std::mutex changedMutex;
    std::vector<AnalysisResult> results;
    results.reserve(func_seq.size());

    auto propagateReady = [&](Function *completed) {
      std::vector<Function *> newlyReady;
      {
        std::lock_guard<std::mutex> dlock(depsMutex);
        for (Function *caller : callGraphState_.getCallers(completed)) {
          if (callGraphState_.isBackEdge(caller, completed))
            continue;
          auto it = pendingCallees.find(caller);
          if (it == pendingCallees.end() || it->second == 0)
            continue;
          it->second--;
          if (it->second == 0)
            newlyReady.push_back(caller);
        }
      }
      if (!newlyReady.empty()) {
        std::lock_guard<std::mutex> qlock(queueMutex);
        ready.insert(ready.end(), newlyReady.begin(), newlyReady.end());
      }
    };

    auto worker = [&]() {
      while (true) {
        Function *func = nullptr;
        {
          std::lock_guard<std::mutex> qlock(queueMutex);
          if (!ready.empty()) {
            func = ready.back();
            ready.pop_back();
          }
        }

        if (!func)
          break;

        bool needsAnalysis = (iteration == 0) || changed_func.count(func);
        AnalysisResult res{func, intraResults_[func], nullptr, false, false};

        if (!needsAnalysis) {
          res.Skipped = true;
        } else {
          std::unique_ptr<IntraLotusAA> new_result(new IntraLotusAA(func, this));
          new_result->computePTA();
          if (lotus_cg)
            new_result->computeCG();

          res.InterfaceChanged =
              res.OldResult ? !res.OldResult->isSameInterface(new_result.get())
                            : true;
          res.NewResult = new_result.release();
        }

        {
          std::lock_guard<std::mutex> rlock(resultsMutex);
          results.push_back(res);
        }

        propagateReady(func);
      }
    };

    std::vector<std::shared_future<void>> futures;
    futures.reserve(poolMax);
    for (unsigned i = 0; i < poolMax; ++i) {
      futures.emplace_back(threadPool.async(worker));
    }
    for (auto &f : futures)
      f.get();

    // Merge results sequentially to avoid concurrent writes to shared maps
    for (AnalysisResult &res : results) {
      if (!res.Skipped && res.NewResult) {
        intraResults_[res.Func] = res.NewResult;
        if (res.OldResult && res.OldResult != res.NewResult)
          delete res.OldResult;
      }

      if (res.InterfaceChanged) {
        changed = true;
        for (Function *caller : callGraphState_.getCallers(res.Func)) {
          if (!callGraphState_.isBackEdge(caller, res.Func)) {
            changed_func.insert(caller);
          }
        }
      }
    }

    outs() << "\n";

    // Update CG if enabled
    if (lotus_cg) {
      // Save callers that need reanalysis before clearing changed_func
      // These are functions whose callees had interface changes
      set<Function *> callersNeedingReanalysis = changed_func;
      changed_func.clear();
      
      for (int i = func_seq.size() - 1; i >= 0; i--) {
        Function *func = func_seq[i];
        IntraLotusAA *func_result = getPtGraph(func);
        
        if (!func_result)
          continue;

        // Get new call graph resolution results
        const auto &newCgResults = func_result->cg_resolve_result;
        
        // Update function pointer results and detect changes
        for (const auto &callsiteResult : newCgResults) {
          Value *callsite = callsiteResult.first;
          const CallTargetSet &newTargets = callsiteResult.second;
          
          CallTargetSet *oldTargets = functionPointerResults_.getTargets(func, callsite);
          
          // Check if changed
          bool targetsChanged = false;
          if (!oldTargets) {
            targetsChanged = !newTargets.empty();
          } else {
            if (oldTargets->size() != newTargets.size()) {
              targetsChanged = true;
            } else {
              for (Function *newTarget : newTargets) {
                if (oldTargets->count(newTarget) == 0) {
                  targetsChanged = true;
                  break;
                }
              }
            }
          }

          if (targetsChanged) {
            changed_func.insert(func);
            changed = true;
          }

          // Update targets
          functionPointerResults_.setTargets(func, callsite, newTargets);
          
          // Update call graph edges
          for (Function *target : newTargets) {
            callGraphState_.addEdge(func, target);
          }
        }
      }

      // Detect back edges in updated call graph
      callGraphState_.detectBackEdges(changed_func);
      
      // Merge back callers that need reanalysis due to interface changes
      changed_func.insert(callersNeedingReanalysis.begin(), callersNeedingReanalysis.end());
      
      if (!changed_func.empty())
        changed = true;
    } else {
      break;  // No CG updates, single iteration
    }

    iteration++;
  }

  outs() << "[LotusAA] Analysis complete\n";
}

void LotusAA::finalizeCg(std::vector<Function *> &func_seq) {
  if (lotus_print_cg) {
    for (Function *func : func_seq) {
      IntraLotusAA *result = getPtGraph(func);
      if (result) {     
        result->showFunctionPointers();
      }
    }
  }

  if (lotus_print_pts) {
    for (Function *func : func_seq) {
      IntraLotusAA *result = getPtGraph(func);
      if (result) {
        result->show();
      }
    }
  }
}

bool LotusAA::computePTA(Function *F) {
  assert(intraResults_.count(F));
  // FIXME: it seems that we almost re-run the analysis in each round of the on-the-fly callgraph construction??
  // Maybe this is due partly to the flow-sensitive nature of our analysis?
  
  IntraLotusAA *old_result = intraResults_[F];
  IntraLotusAA *new_result = new IntraLotusAA(F, this);
  
  new_result->computePTA();

  if (lotus_cg)
    new_result->computeCG();

  bool interface_changed = true;
  if (old_result) {
    interface_changed = !old_result->isSameInterface(new_result);
    delete old_result;
  }

  intraResults_[F] = new_result;
  return interface_changed;
}

IntraLotusAA *LotusAA::getPtGraph(Function *F) {
  auto it = intraResults_.find(F);
  return (it == intraResults_.end()) ? nullptr : it->second;
}

DominatorTree *LotusAA::getDomTree(Function *F) {
  std::lock_guard<std::mutex> lock(domMutex_);

  // Check if already computed
  auto it = dominatorTrees_.find(F);
  if (it != dominatorTrees_.end())
    return it->second;

  // External functions (declarations) have no body, so no dominator tree
  if (F->isDeclaration()) {
    dominatorTrees_[F] = nullptr;
    return nullptr;
  }

  // Compute dominator tree for this function
  DominatorTree *DT = new DominatorTree(*F);
  dominatorTrees_[F] = DT;
  return DT;
}

bool LotusAA::isBackEdge(Function *caller, Function *callee) {
  return callGraphState_.isBackEdge(caller, callee);
}

CallTargetSet *LotusAA::getCallees(Function *func, Value *callsite) {
  return functionPointerResults_.getTargets(func, callsite);
}

