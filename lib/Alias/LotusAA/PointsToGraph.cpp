/*
 * LotusAA - Points-To Graph 
 */

#include "Alias/LotusAA/PointsToGraph.h"
#include "Alias/LotusAA/InterProceduralPass.h"
#include "Alias/LotusAA/Config.h"

#include <llvm/Analysis/DominanceFrontier.h>
#include <llvm/Support/CommandLine.h>

using namespace llvm;
using namespace std;

static cl::opt<int> lotus_restrict_pts_count(
    "lotus-restrict-pts-count",
    cl::desc("Maximum number of locators a pointer may point to"),
    cl::init(100), cl::Hidden);

static cl::opt<int> lotus_restrict_obj_ap_depth(
    "lotus-restrict-obj-ap-depth",
    cl::desc("Maximum AP-depth of objects considered for callees"),
    cl::init(5), cl::Hidden);

// Static members
Type *PTGraph::DEFAULT_NON_POINTER_TYPE = nullptr;
Type *PTGraph::DEFAULT_POINTER_TYPE = nullptr;
const int PTGraph::VALUE_SEQ_UNDEF = -1;
const int PTGraph::VALUE_SEQ_INFINITE = -2;
const int PTGraph::FUNC_OBJ_UNREACHABLE = -1;

// PTResultIterator
PTResultIterator::PTResultIterator(PTResult *target, PTGraph *parent_graph)
    : parent_graph(parent_graph) {
  set<PTResult *> visited;
  visit(target, 0, visited);
  
  // Optimize: cache results in target
  if (!target->is_optimized) {
    target->pt_list.clear();
    int count = 0;
    for (auto loc : res) {
      count++;
      if (lotus_restrict_pts_count != -1 && count > lotus_restrict_pts_count)
        break;
      target->pt_list.push_back(PTResult::PtItem(loc));
    }
    target->is_optimized = true;
  }
}

void PTResultIterator::visit(PTResult *target, int64_t off,
                             set<PTResult *> &visited) {
  assert(target && "Null target");
  
  // Check for cycles - if already visited, skip
  if (visited.count(target))
    return;
  
  visited.insert(target);

  // Direct targets
  for (PTResult::PtItem &item : target->pt_list) {
    ObjectLocator *locator = item.locator->offsetBy(off);
    res.insert(locator);
  }

  // Derived targets
  for (PTResult::DerivedPtItem &item : target->derived_list) {
    visit(item.src_pts, off + item.offset, visited);
  }
  
  // Don't erase from visited - we want to prevent cycles
}

namespace llvm {

raw_ostream &operator<<(raw_ostream &out, PTResultIterator &pt_it) {
  for (auto it = pt_it.begin(); it != pt_it.end(); ++it) {
    out << "  " << **it << "\n";
  }
  return out;
}

} // namespace llvm (for operator<<)

// PTGraph
PTGraph::PTGraph(Function *F, LotusAA *lotus_aa)
    : analyzed_func(F), lotus_aa(lotus_aa),
      pt_index(0), obj_index(0), load_load_match_performed(false) {
  
  // Get dominance information
  dom_tree = lotus_aa->getDomTree(F);

  // Create NULL points-to result
  NullPTS = addPointsTo(nullptr, MemObject::NullObj, 0);
}

PTGraph::~PTGraph() {
  delete NullPTS;

  for (auto &it : pt_results) {
    if (it.second != NullPTS)
      delete it.second;
  }

  for (auto &obj : mem_objs) {
    delete obj.first;
  }

  for (auto &category : load_category_collection) {
    delete category;
  }
}

PTResult *PTGraph::findPTResult(Value *ptr, bool is_create) {
  auto it = pt_results.find(ptr);
  if (it != pt_results.end())
    return it->second;

  if (is_create) {
    PTResult *pts = new PTResult(ptr);
    pt_results[ptr] = pts;
    return pts;
  }

  return nullptr;
}

MemObject *PTGraph::newObject(Value *alloc_site, MemObject::ObjKind obj_type) {
  MemObject *obj = (obj_type == MemObject::CONCRETE) 
                    ? new MemObject(alloc_site, this, obj_type)
                    : new SymbolicMemObject(alloc_site, this);

  if (alloc_site && isa<GlobalValue>(alloc_site)) {
    global_objects.insert(obj);
  }

  mem_objs[obj] = obj_index++;
  return obj;
}

PTResult *PTGraph::addPointsTo(Value *ptr, MemObject *obj, int64_t offset) {
  assert(pt_results.find(ptr) == pt_results.end() && "Re-assigning value (SSA violation)");
  PTResult *pts = findPTResult(ptr, true);
  pts->add_target(obj, offset);
  return pts;
}

PTResult *PTGraph::derivePtsFrom(Value *ptr, PTResult *other_pts, int64_t offset) {
  assert(pt_results.find(ptr) == pt_results.end() && "Re-assigning value (SSA violation)");
  PTResult *pts = findPTResult(ptr, true);
  pts->add_derived_target(other_pts, offset);
  return pts;
}

PTResult *PTGraph::assignPts(Value *ptr, PTResult *pts) {
  pt_results[ptr] = pts;
  return pts;
}

Type *PTGraph::normalizeType(Type *type) {
  assert(type && "Normalizing NULL type");
  return type;  // Use original type
}

void PTGraph::refineResult(mem_value_t &to_refine) {
  map<pair<Value *, Instruction *>, bool> seen;
  mem_value_t refined;

  for (auto &item : to_refine) {
    auto key = make_pair(item.val, item.pos);
    if (!seen.count(key)) {
      seen[key] = true;
      refined.push_back(item);
    }
  }

  to_refine = std::move(refined);
}

void PTGraph::trackPtrRightValue(Value *ptr, mem_value_t &res) {
  set<Value *, llvm_cmp> visited;
  trackPtrRightValueImpl(ptr, res, visited);
}

void PTGraph::trackPtrRightValueImpl(Value *ptr, mem_value_t &res,
                                      set<Value *, llvm_cmp> &visited) {
  // Cycle detection - prevent infinite recursion
  if (visited.count(ptr))
    return;
  
  visited.insert(ptr);

  if (Argument *arg = dyn_cast<Argument>(ptr)) {
    res.push_back(mem_value_item_t(nullptr, arg));
  } else if (LoadInst *load = dyn_cast<LoadInst>(ptr)) {
    mem_value_t load_result;
    getLoadValues(load->getPointerOperand(), load, load_result);
    for (auto &item : load_result) {
      trackPtrRightValueImpl(item.val, res, visited);
    }
  } else if (PHINode *phi = dyn_cast<PHINode>(ptr)) {
    for (unsigned i = 0; i < phi->getNumIncomingValues(); i++) {
      trackPtrRightValueImpl(phi->getIncomingValue(i), res, visited);
    }
  } else if (SelectInst *sel = dyn_cast<SelectInst>(ptr)) {
    trackPtrRightValueImpl(sel->getTrueValue(), res, visited);
    
    trackPtrRightValueImpl(sel->getFalseValue(), res, visited);
  } else if (CastInst *cast = dyn_cast<CastInst>(ptr)) {
    trackPtrRightValueImpl(cast->getOperand(0), res, visited);
  } else {
    res.push_back(mem_value_item_t(nullptr, ptr));
  }

  refineResult(res);
}

void PTGraph::getLoadValues(Value *ptr, Instruction *from_loc,
                               mem_value_t &res, int64_t offset) {
  loadPtrAt(ptr, from_loc, res, false, offset);
}

void PTGraph::loadPtrAt(Value *ptr, Instruction *from_loc, mem_value_t &result,
                          bool create_symbol, int64_t query_offset) {
  PTResult *ptr_pts = findPTResult(ptr);
  assert(ptr_pts && "Load from NULL pointer");

  Type *value_type = nullptr;
  if (create_symbol) {
    PointerType *ptr_type = dyn_cast<PointerType>(ptr->getType());
    if (ptr_type) {
      value_type = getPointerElementTypeCompat(ptr_type, &getDL());
      // Adjust type for offset if needed
      // Simplified: just use the element type
    } else {
      value_type = DEFAULT_NON_POINTER_TYPE;
    }
  }

  // Collect all points-to targets
  PTResultIterator iter(ptr_pts, this);
  if (lotus_restrict_pts_count != -1 && iter.size() > lotus_restrict_pts_count)
    return;

  for (auto loc : iter) {
    int64_t offset = loc->getOffset();
    MemObject *obj = loc->getObj();
    
    // Skip null and unknown objects
    if (obj->isNull() || obj->isUnknown())
      continue;

    // Adjust offset if query_offset provided
    if (query_offset != 0) {
      loc = obj->findLocator(offset + query_offset, true);
    }

    // Get values from this locator
    mem_value_t tmp_result;
    loc->getValues(from_loc, tmp_result, value_type,
                   ObjectLocator::FUNC_LEVEL_UNDEFINED, true);

    result.insert(result.end(), tmp_result.begin(), tmp_result.end());

    // Track loaded values for load instructions
    if (isa<LoadInst>(from_loc) || isa<CallBase>(from_loc)) {
      Value *val = from_loc;
      if (isa<LoadInst>(from_loc))
        val = from_loc;
      obj->getLoadedValues()[offset + query_offset].insert(val);
    }
  }
}

bool PTGraph::cacheLoadCategory(LoadInst *load_inst) {
  for (unsigned idx = 0; idx < load_category_collection.size(); idx++) {
    assert(!load_category_collection[idx]->empty());
    LoadInst *rep = *load_category_collection[idx]->begin();
    if (isSameValue(load_inst, rep)) {
      load_category_collection[idx]->insert(load_inst);
      load_category[load_inst] = idx;
      return false;
    }
  }

  // New category
  load_category[load_inst] = load_category_collection.size();
  auto *new_category = new set<LoadInst *, llvm_cmp>;
  new_category->insert(load_inst);
  load_category_collection.push_back(new_category);
  return true;
}

void PTGraph::performLoadLoadMatch() {
  if (load_load_match_performed)
    return;

  for (BasicBlock &B : *analyzed_func) {
    for (Instruction &I : B) {
      if (LoadInst *load = dyn_cast<LoadInst>(&I)) {
        cacheLoadCategory(load);
      }
    }
  }
  
  load_load_match_performed = true;
}

const set<LoadInst *, llvm_cmp> &
PTGraph::getAllLoadWithSameValue(LoadInst *load_inst) {
  assert(load_category.count(load_inst));
  int idx = load_category[load_inst];
  return *load_category_collection[idx];
}

bool PTGraph::isSameValue(LoadInst *l1, LoadInst *l2) {
  if (!load_category.empty() && load_category.count(l1) && load_category.count(l2)) {
    return load_category[l1] == load_category[l2];
  }
  return isSameValue(l1->getPointerOperand(), l1, l2->getPointerOperand(), l2);
}

bool PTGraph::isSameValue(Value *ptr1, Instruction *pos1, Value *ptr2,
                            Instruction *pos2, int64_t offset1,
                            int64_t offset2) {
  PTResult *ptr1_pts = findPTResult(ptr1);
  PTResult *ptr2_pts = findPTResult(ptr2);

  if (!ptr1_pts || !ptr2_pts)
    return false;

  PTResultIterator iter1(ptr1_pts, this);
  PTResultIterator iter2(ptr2_pts, this);

  if (iter1.size() != iter2.size() || iter1.size() == 0)
    return false;

  // Check if all locations match
  set<ObjectLocator *, obj_loc_cmp> locs1, locs2;
  for (auto loc : iter1)
    locs1.insert(loc->offsetBy(offset1));
  for (auto loc : iter2)
    locs2.insert(loc->offsetBy(offset2));

  if (locs1 != locs2)
    return false;

  // Check if versions match
  for (auto loc : locs1) {
    MemObject *obj = loc->getObj();
    if (obj->isNull() || obj->isUnknown())
      continue;

    if (loc->getVersion(pos1) != loc->getVersion(pos2))
      return false;
  }

  return true;
}

void PTGraph::dumpMemObjs() {
  for (auto &pair : mem_objs) {
    outs() << "ID:" << pair.second << "\n";
    pair.first->dump();
  }
}

int PTGraph::getObjectToCallApDepth(MemObject *obj, CallInst *call) {
  if (!obj || !call)
    return FUNC_OBJ_UNREACHABLE;

  // Get or compute frontier
  set<MemObject *, mem_obj_cmp> &frontier = object_call_ap_depth_frontier[call];
  map<MemObject *, int, mem_obj_cmp> &cache = object_call_arg_ap_depth_cache[call];
  
  if (frontier.empty()) {
    // Initialize: add global objects
    for (MemObject *global_obj : global_objects) {
      frontier.insert(global_obj);
      cache[global_obj] = 1;
    }

    // Add objects reachable from call arguments
    for (unsigned i = 0; i < call->arg_size(); i++) {
      Value *arg = call->getArgOperand(i);
      PTResult *pts_result = findPTResult(arg, false);
      if (pts_result) {
        PTResultIterator result_iter(pts_result, this);
        for (auto pt_loc : result_iter) {
          MemObject *pt_obj = pt_loc->getObj();
          if (!cache.count(pt_obj)) {
            cache[pt_obj] = 1;
            frontier.insert(pt_obj);
          }
        }
      }
    }
  }

  // Check cache
  auto cache_find = cache.find(obj);
  if (cache_find != cache.end())
    return cache_find->second;

  // Not in cache - compute on demand
  if (frontier.empty()) {
    cache[obj] = FUNC_OBJ_UNREACHABLE;
    return FUNC_OBJ_UNREACHABLE;
  }

  // Get frontier depth
  MemObject *frontier_sample = *frontier.begin();
  int frontier_depth = cache[frontier_sample];

  // Expand frontier until target found or max depth reached
  set<MemObject *, mem_obj_cmp> new_frontier;
  while (frontier_depth < lotus_restrict_obj_ap_depth) {
    for (MemObject *frontier_obj : frontier) {
      map<int64_t, Type *> &updated_offsets = frontier_obj->getUpdatedOffset();
      
      for (auto &offset_pair : updated_offsets) {
        int64_t offset = offset_pair.first;
        ObjectLocator *locator = frontier_obj->findLocator(offset, false);
        if (locator) {
          mem_value_t pt_values;
          locator->getValues(call, pt_values);
          
          for (mem_value_item_t &value_item : pt_values) {
            Value *val = value_item.val;
            PTResult *pts_result = findPTResult(val, false);
            if (pts_result) {
              PTResultIterator result_iter(pts_result, this);
              for (auto pt_loc : result_iter) {
                MemObject *pt_obj = pt_loc->getObj();
                if (!cache.count(pt_obj)) {
                  cache[pt_obj] = frontier_depth + 1;
                  new_frontier.insert(pt_obj);
                }
              }
            }
          }
        }
      }
    }

    frontier.clear();
    for (MemObject *mem_obj : new_frontier) {
      frontier.insert(mem_obj);
    }
    new_frontier.clear();

    // Check if target found
    cache_find = cache.find(obj);
    if (cache_find != cache.end())
      return cache_find->second;

    frontier_depth++;
  }

  cache[obj] = FUNC_OBJ_UNREACHABLE;
  return FUNC_OBJ_UNREACHABLE;
}

const DataLayout &PTGraph::getDL() {
  return lotus_aa->getDataLayout();
}

