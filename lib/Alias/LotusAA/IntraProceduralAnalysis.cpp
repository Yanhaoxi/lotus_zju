#include "Alias/LotusAA/IntraProceduralAnalysis.h"

#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Operator.h>
#include <llvm/IR/CFG.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;
using namespace std;

// Configuration
int IntraLotusAAConfig::lotus_restrict_inline_depth = 2;
double IntraLotusAAConfig::lotus_timeout = 10.0;
int IntraLotusAAConfig::lotus_restrict_cg_size = 5;
bool IntraLotusAAConfig::lotus_test_correctness = false;
int IntraLotusAAConfig::lotus_restrict_inline_size = 100;
int IntraLotusAAConfig::lotus_restrict_ap_level = 2;

static cl::opt<int> lotus_restrict_inline_depth_cl(
    "lotus-restrict-inline-depth",
    cl::desc("Maximum inlining depth for inter-procedural analysis"),
    cl::init(2), cl::Hidden);

static cl::opt<int> lotus_restrict_cg_size_cl(
    "lotus-restrict-cg-size",
    cl::desc("Maximum indirect call targets to process"),
    cl::init(5), cl::Hidden);

void IntraLotusAAConfig::setParam() {
  if (lotus_restrict_inline_depth_cl.getNumOccurrences() > 0)
    lotus_restrict_inline_depth = lotus_restrict_inline_depth_cl;
  if (lotus_restrict_cg_size_cl.getNumOccurrences() > 0)
    lotus_restrict_cg_size = lotus_restrict_cg_size_cl;
}

// IntraLotusAA implementation
const int IntraLotusAA::PTR_TO_ESC_OBJ = -1;

IntraLotusAA::IntraLotusAA(Function *F, LotusAA *lotus_aa)
    : PTGraph(F, lotus_aa), func_obj(nullptr), func_new(nullptr),
      is_PTA_computed(false), is_CG_computed(false),
      is_considered_as_library(false), is_timeout_found(false),
      inline_ap_depth(0) {
  
  getReturnInst();
  
  // Topological sort of BBs (simple RPO)
  for (BasicBlock &BB : *F) {
    topBBs.push_back(&BB);
  }
}

IntraLotusAA::~IntraLotusAA() {
  for (OutputItem *item : outputs) {
    delete item;
  }
}

// Utility functions now in IntraLotusAAUtils.cpp

PTResult *IntraLotusAA::processPhi(PHINode *phi) {
  PTResult *phi_pts = findPTResult(phi, true);

  for (unsigned i = 0; i < phi->getNumIncomingValues(); i++) {
    Value *val_i = phi->getIncomingValue(i);
    PTResult *in_pts = processBasePointer(val_i);
    assert(in_pts && "PHI incoming value not processed");
    phi_pts->add_derived_target(in_pts, 0);
  }

  PTResultIterator iter(phi_pts, this);
  return phi_pts;
}

void IntraLotusAA::processLoad(LoadInst *load_inst) {
  Value *load_ptr = load_inst->getPointerOperand();
  processBasePointer(load_ptr);

  if (!load_inst->getType()->isPointerTy())
    return;

  mem_value_t result;
  loadPtrAt(load_ptr, load_inst, result, true);

  PTResult *load_pts = findPTResult(load_inst, true);

  for (auto &load_pair : result) {
    Value *fld_val = load_pair.val;

    if (fld_val == LocValue::FREE_VARIABLE ||
        fld_val == LocValue::UNDEF_VALUE ||
        fld_val == LocValue::SUMMARY_VALUE)
      continue;

    PTResult *fld_pts = processBasePointer(fld_val);
    load_pts->add_derived_target(fld_pts, 0);
  }
  
  PTResultIterator iter(load_pts, this);
}

void IntraLotusAA::processStore(StoreInst *store) {
  Value *ptr = store->getPointerOperand();
  Value *store_value = store->getValueOperand();
  PTResult *res = processBasePointer(ptr);
  assert(res && "Store pointer not processed");

  PTResultIterator iter(res, this);

  for (auto loc : iter) {
    MemObject *obj = loc->getObj();
    if (obj->isNull() || obj->isUnknown())
      continue;

    loc->storeValue(store_value, store, 0);
  }

  if (store_value->getType()->isPointerTy()) {
    processBasePointer(store_value);
  }
}

PTResult *IntraLotusAA::processAlloca(AllocaInst *alloca) {
  return addPointsTo(alloca, newObject(alloca), 0);
}

PTResult *IntraLotusAA::processSelect(SelectInst *select) {
  if (!select->getType()->isPointerTy())
    return nullptr;

  Value *true_val = select->getTrueValue();
  Value *false_val = select->getFalseValue();

  PTResult *pts_true = processBasePointer(true_val);
  PTResult *pts_false = processBasePointer(false_val);

  PTResult *select_pts = findPTResult(select, true);
  select_pts->add_derived_target(pts_true, 0);
  select_pts->add_derived_target(pts_false, 0);

  PTResultIterator iter(select_pts, this);
  return select_pts;
}

PTResult *IntraLotusAA::processArg(Argument *arg) {
  MemObject::ObjKind kind = func_pseudo_ret_cache.count(arg) 
                             ? MemObject::CONCRETE 
                             : MemObject::SYMBOLIC;
  return addPointsTo(arg, newObject(arg, kind), 0);
}

PTResult *IntraLotusAA::processGlobal(GlobalValue *global) {
  return addPointsTo(global, newObject(global, MemObject::SYMBOLIC), 0);
}

PTResult *IntraLotusAA::processNullptr(ConstantPointerNull *null_ptr) {
  return assignPts(null_ptr, NullPTS);
}

PTResult *IntraLotusAA::processNonPointer(Value *non_pointer_val) {
  if (CastInst *cast = dyn_cast<CastInst>(non_pointer_val)) {
    Value *src = cast->getOperand(0);
    if (src->getType()->isPointerTy()) {
      PTResult *src_res = processBasePointer(src);
      return derivePtsFrom(non_pointer_val, src_res, 0);
    }
  }
  return addPointsTo(non_pointer_val, newObject(non_pointer_val), 0);
}

PTResult *IntraLotusAA::processUnknown(Value *unknown_val) {
  return addPointsTo(unknown_val, MemObject::UnknownObj, 0);
}

PTResult *IntraLotusAA::processGepBitcast(Value *ptr) {
  // Track pointer through GEP/bitcast operations
  int64_t offset = 0;
  Value *base_ptr = ptr;

  // For GEP, extract base pointer
  // Note: Offset tracking is intentionally simplified to 0
  // Field-sensitivity is handled through ObjectLocator field tracking,
  // not through offset arithmetic in points-to results
  if (GEPOperator *gep = dyn_cast<GEPOperator>(ptr)) {
    base_ptr = gep->getPointerOperand();
    offset = 0;  // Field offsets handled by ObjectLocator
  } else if (BitCastInst *bc = dyn_cast<BitCastInst>(ptr)) {
    base_ptr = bc->getOperand(0);
    offset = 0;
  }

  if (base_ptr == ptr) {
    return addPointsTo(ptr, newObject(ptr, MemObject::CONCRETE), 0);
  }

  PTResult *pts = processBasePointer(base_ptr);
  PTResult *ret = derivePtsFrom(ptr, pts, offset);
  PTResultIterator iter(ret, this);
  return ret;
}

PTResult *IntraLotusAA::processCast(CastInst *cast) {
  Value *base_ptr = cast->getOperand(0);
  PTResult *pts = processBasePointer(base_ptr);
  PTResult *ret = derivePtsFrom(cast, pts, 0);
  PTResultIterator iter(ret, this);
  return ret;
}

PTResult *IntraLotusAA::processBasePointer(Value *base_ptr) {
  PTResult *res = findPTResult(base_ptr);
  if (res)
    return res;

  if (isa<GEPOperator>(base_ptr) || isa<BitCastInst>(base_ptr)) {
    res = processGepBitcast(base_ptr);
  } else if (CastInst *cast = dyn_cast<CastInst>(base_ptr)) {
    res = processCast(cast);
  } else if (Argument *arg = dyn_cast<Argument>(base_ptr)) {
    res = processArg(arg);
  } else if (ConstantPointerNull *cnull = dyn_cast<ConstantPointerNull>(base_ptr)) {
    res = processNullptr(cnull);
  } else if (GlobalValue *gv = dyn_cast<GlobalValue>(base_ptr)) {
    res = processGlobal(gv);
  } else if (ConstantExpr *ce = dyn_cast<ConstantExpr>(base_ptr)) {
    if (ce->getOpcode() == Instruction::BitCast || 
        ce->getOpcode() == Instruction::GetElementPtr)
      res = processGepBitcast(base_ptr);
  } else if (!base_ptr->getType()->isPointerTy()) {
    res = processNonPointer(base_ptr);
  }

  if (!res)
    res = processUnknown(base_ptr);

  return res;
}

void IntraLotusAA::processUnknownLibraryCall(CallBase *call) {
  // Mark all pointer arguments as potentially modified
  for (unsigned i = 0; i < call->arg_size(); i++) {
    Value *arg = call->getArgOperand(i);
    if (!arg->getType()->isPointerTy())
      continue;

    processBasePointer(arg);
    
    PTResult *pt_result = findPTResult(arg, false);
    if (!pt_result)
      continue;

    PTResultIterator iter(pt_result, this);
    for (auto loc : iter) {
      loc->storeValue(LocValue::NO_VALUE, call, 0);
    }
  }
}

void IntraLotusAA::processCall(CallBase *call) {
  if (IntraLotusAAConfig::lotus_restrict_inline_depth == 0) {
    if (call->getType()->isPointerTy()) {
      addPointsTo(call, newObject(call, MemObject::CONCRETE), 0);
    }
    return;
  }

  Function *base_func = call->getParent()->getParent();
  auto callees = lotus_aa->getCallees(base_func, call);

  if (!callees) {
    processUnknownLibraryCall(call);
    return;
  }

  // Process each possible callee
  int callee_idx = 0;
  for (Function *callee : *callees) {
    if (callee_idx >= IntraLotusAAConfig::lotus_restrict_cg_size)
      break;

    if (!callee || lotus_aa->isBackEdge(base_func, callee)) {
      if (call->getType()->isPointerTy() && (size_t)callee_idx == callees->size() - 1) {
        if (!pt_results.count(call))
          addPointsTo(call, newObject(call, MemObject::CONCRETE), 0);
      }
      callee_idx++;
      continue;
    }

    IntraLotusAA *callee_result = lotus_aa->getPtGraph(callee);

    if (!callee_result || callee_result->is_considered_as_library) {
      if (call->getType()->isPointerTy() && (size_t)callee_idx == callees->size() - 1) {
        if (!pt_results.count(call))
          addPointsTo(call, newObject(call, MemObject::CONCRETE), 0);
      }
      processUnknownLibraryCall(call);
      callee_idx++;
      continue;
    }

    // Process callee summary: inputs, outputs, and escaped objects
    auto &callee_inputs = callee_result->getInputs();
    auto &callee_outputs = callee_result->getOutputs();
    auto &callee_escape = callee_result->getEscapeObjs();

    func_arg_t &arg_result = func_arg[call][callee];

    std::vector<Value *> formal_args, real_args;
    for (Argument &arg : callee->args()) {
      formal_args.push_back(&arg);
    }
    for (unsigned i = 0; i < call->arg_size(); i++) {
      real_args.push_back(call->getArgOperand(i));
    }

    processCalleeInput(callee_inputs, callee_result->inputs_func_level,
                        real_args, formal_args, call, arg_result);
    processCalleeOutput(callee_outputs, callee_escape, call, callee);

    callee_idx++;
  }
}

void IntraLotusAA::processCalleeInput(
    map<Value *, AccessPath, llvm_cmp> &callee_input,
    map<Value *, int, llvm_cmp> &/*callee_input_func_level*/,
    std::vector<Value *> &real_args, std::vector<Value *> &formal_args,
    CallBase *callsite, func_arg_t &result) {

  // (1) Collect the real arguments and link the values to pseudo-arguments
  int real_size = real_args.size();
  int formal_size = formal_args.size();
  for (int idx = 0; idx < real_size && idx < formal_size; idx++) {
    Value *formal_arg = formal_args[idx];
    Value *real_arg = real_args[idx];
    
    mem_value_item_t mem_val_item(nullptr, real_arg);
    result[formal_arg].push_back(mem_val_item);

    if (real_arg->getType()->isPointerTy()) {
      processBasePointer(real_arg);
    }
  }

  // (2) Process the side-effect inputs
  // For each input ptr->idx1->idx2->idx3
  // We first check if it is already processed
  // If it is not processed, we use the value of ptr->idx1->idx2 and compute the
  // value of ptr->idx1->idx2->idx3 If ptr->idx1->idx2 also does not exist, we
  // first use ptr->idx1 to compute ptr->idx1->idx2 We keep doing these steps
  // until the required value is computed or is a global or argument
  set<Value *, llvm_cmp> processed;
  for (auto &iter : callee_input) {
    Value *pseudo_arg = iter.first;
    if (processed.count(pseudo_arg)) {
      continue;
    }

    std::vector<Value *> parents;
    Value *parent_iter = pseudo_arg;
    while ((!processed.count(parent_iter)) &&
           (callee_input.count(parent_iter))) {
      parents.push_back(parent_iter);
      AccessPath &parent_info = callee_input[parent_iter];
      Value *parent_arg = parent_info.getParentPtr();
      parent_iter = parent_arg;
    }

    for (int i = parents.size() - 1; i >= 0; i--) {
      Value *curr_arg_val = parents[i];
      processed.insert(curr_arg_val);
      assert(callee_input.count(curr_arg_val) && "Invalid Value found");
      AccessPath &arg_info = callee_input[curr_arg_val];

      Value *parent_arg = arg_info.getParentPtr();
      int64_t offset = arg_info.getOffset();

      mem_value_t &parent_arg_values = result[parent_arg];

      if (!isPseudoInput(parent_arg)) {
        // The parent arg is a real Argument or a Global Value
        processBasePointer(parent_arg);
        if (isa<GlobalValue>(parent_arg)) {
          // Process the global values on demand
          mem_value_item_t mem_value_item(nullptr, parent_arg);
          parent_arg_values.push_back(mem_value_item);
        } else if (isa<Argument>(parent_arg)) {
          // Arguments are processed before
        } else {
          // Default
        }
      }

      refineResult(parent_arg_values);

      mem_value_t &arg_values = result[curr_arg_val];
      for (auto &parent_value_pair : parent_arg_values) {
        Value *parent_value = parent_value_pair.val;
        if (parent_value == LocValue::FREE_VARIABLE ||
            parent_value == LocValue::UNDEF_VALUE ||
            parent_value == LocValue::SUMMARY_VALUE) {
          continue;
      }

      mem_value_t tmp_values;

        if (findPTResult(parent_value) == nullptr) {
          if (isa<Argument>(parent_value)) {
            // Only when the parent value is an argument (Real Argument/ Side
            // effect input/ Output from callee), we create a new object
            Argument *parent_value_to_arg = dyn_cast<Argument>(parent_value);
            processArg(parent_value_to_arg);
          } else {
            continue;
          }
        }
        loadPtrAt(parent_value, callsite, tmp_values, true, offset);

        for (auto &tmp_val : tmp_values) {
          mem_value_item_t mem_value_item(nullptr, tmp_val.val);
          arg_values.push_back(mem_value_item);
        }
      }
    refineResult(arg_values);
    }
  }
}

void IntraLotusAA::processCalleeOutput(
    std::vector<OutputItem *> &callee_output,
    set<MemObject *, mem_obj_cmp> &callee_escape,
    Instruction *callsite, Function *callee) {

  auto &func_arg_all = func_arg[callsite];

  if (!func_arg_all.count(callee)) {
    // Inputs for callee function is not processed
    return;
  }

  func_arg_t &callee_func_arg = func_arg_all[callee];

  // (1) Create pseudo-nodes for return value and the side-effect outputs
  // Each pseudo-node is an Argument
  assert(!func_ret[callsite].count(callee) && "callsite already processed!!!");

  std::vector<Value *> &out_values = func_ret[callsite][callee];
  out_values.push_back(callsite);
  for (size_t idx = 1; idx < callee_output.size(); idx++) {
    OutputItem *output = callee_output[idx];
    Type *output_type = output->getType();

    string name_str;
    raw_string_ostream ss(name_str);
    ss << "LPseudoCallSiteOutput_" << callsite << "_" << callee << "_#" << idx;
    ss.flush();

    Argument *new_arg = new Argument(output_type, name_str);
    out_values.push_back(new_arg);
    func_pseudo_ret_cache[new_arg] = make_pair(callsite, idx);
  }

  assert(out_values.size() == callee_output.size() &&
         "Incorrect collection of outputs");

  // (2) Create the objects that escape to this caller function
  map<Value *, MemObject *, llvm_cmp> escape_object_map;
  int escape_obj_idx = 0;

  for (MemObject *callee_escape_obj : callee_escape) {
    if (callee_escape_obj == nullptr) {
      continue;
    }

    Value *alloca_site = callee_escape_obj->getAllocSite();
    if (alloca_site == nullptr) {
      // Null Objects and Unknown Objects are not processed
      continue;
    }
    Type *obj_ptr_type = alloca_site->getType();

    string name_str;
    raw_string_ostream ss(name_str);
    ss << "LCallSiteEscapedObject_" << callsite << "_#" << escape_obj_idx++;
    ss.flush();

    Argument *new_arg = new Argument(obj_ptr_type, name_str);
    func_pseudo_ret_cache[new_arg] = make_pair(callsite, PTR_TO_ESC_OBJ);
    MemObject::ObjKind obj_kind = MemObject::CONCRETE;
    MemObject *escaped_obj_to = newObject(new_arg, obj_kind);
    addPointsTo(new_arg, escaped_obj_to, 0);
    escape_object_map[alloca_site] = escaped_obj_to;
    
    // Cache the escape mapping
    func_escape[callsite][callee][callee_escape_obj] = escaped_obj_to;
  }

  std::set<PTResult *> visited;
  std::unordered_map<PTResult *, PTResultIterator> pt_result_cache;
  
  for (size_t idx = 0; idx < callee_output.size(); idx++) {
    // (3) Link the point-to results for pseudo outputs
    OutputItem *output = callee_output[idx];
    auto &callee_point_to = output->getPseudoPointTo();
    AccessPath output_info = output->getSymbolicInfo();
    Value *output_parent = output_info.getParentPtr();
    int64_t output_offset = output_info.getOffset();
    Value *curr_output = out_values[idx];
    PTResult *curr_output_pts = nullptr;
    int func_level = output->getFuncLevel();

    if (func_level == ObjectLocator::FUNC_LEVEL_UNDEFINED) {
      func_level = 0;
      output->func_level = 0;
    }

    // Link the pointer-result and the values
    for (auto &callee_point_to_item_info : callee_point_to) {
      Value *callee_point_to_item_parent_ptr = callee_point_to_item_info.getParentPtr();
      int64_t callee_point_to_item_offset = callee_point_to_item_info.getOffset();
      
      if (callee_point_to_item_parent_ptr == nullptr) {
        // Pointer pointing to null or unknown object
        curr_output_pts = curr_output_pts ? curr_output_pts : findPTResult(curr_output, true);
        curr_output_pts->add_target(MemObject::UnknownObj, callee_point_to_item_offset);
      } else if (isa<GlobalValue>(callee_point_to_item_parent_ptr)) {
        PTResult *linked_pts = processBasePointer(callee_point_to_item_parent_ptr);
        curr_output_pts = curr_output_pts ? curr_output_pts : findPTResult(curr_output, true);
        curr_output_pts->add_derived_target(linked_pts, callee_point_to_item_offset);
      } else if (escape_object_map.count(callee_point_to_item_parent_ptr)) {
        // Escaped_obj from callee
        MemObject *curr_obj = escape_object_map[callee_point_to_item_parent_ptr];
        curr_output_pts = curr_output_pts ? curr_output_pts : findPTResult(curr_output, true);
        curr_output_pts->add_target(curr_obj, callee_point_to_item_offset);
      } else {
        // The point-to object is from the analyzed function (caller function)
        if (!callee_func_arg.count(callee_point_to_item_parent_ptr))
          continue;
          
        auto &callee_arg_vals = callee_func_arg[callee_point_to_item_parent_ptr];

        if (!callee_arg_vals.empty()) {
          curr_output_pts = curr_output_pts ? curr_output_pts : findPTResult(curr_output, true);
          visited.emplace(curr_output_pts);
        }
        for (auto &arg_point_to : callee_arg_vals) {
          Value *pointer = arg_point_to.val;

          PTResult *linked_pts = processBasePointer(pointer);
          curr_output_pts->add_derived_target(linked_pts, callee_point_to_item_offset);
        }
      }
    }
    
    for (PTResult *visited_item : visited) {
      if (!pt_result_cache.count(visited_item)) {
        PTResultIterator iter(visited_item, this);
        pt_result_cache.emplace(visited_item, std::move(iter));
      }
    }

    // (4) Link the value
    if (idx != 0) {
      // idx=0 means that the real return value, which do not need special linkage
      if (escape_object_map.count(output_parent)) {
        // Escaped_obj from callee
        MemObject *curr_obj = escape_object_map[output_parent];
        ObjectLocator *locator = curr_obj->findLocator(output_offset, true);
        locator->storeValue(curr_output, callsite, 0);
      } else {
        if (!callee_func_arg.count(output_parent))
          continue;

        auto &callee_arg_vals = callee_func_arg[output_parent];

        if (callee_arg_vals.empty() && isa<GlobalValue>(output_parent)) {
          mem_value_item_t global_value(nullptr, output_parent);
          callee_arg_vals.push_back(global_value);
        }

        for (auto &arg_point_to : callee_arg_vals) {
          Value *pointer = arg_point_to.val;
          if (pointer == LocValue::FREE_VARIABLE) {
            continue;
          }

          PTResult *pt_res = findPTResult(pointer);
          if (pt_res == nullptr) {
            if (isa<Argument>(pointer)) {
              Argument *parent_value_to_arg = dyn_cast<Argument>(pointer);
              pt_res = processArg(parent_value_to_arg);
            } else if (isa<GlobalValue>(pointer)) {
              GlobalValue *global = dyn_cast<GlobalValue>(pointer);
              pt_res = processGlobal(global);
            } else {
              continue;
            }
          }

          if (!pt_result_cache.count(pt_res)) {
            PTResultIterator pt_iter(pt_res, this);
            pt_result_cache.emplace(pt_res, std::move(pt_iter));
          }

          for (auto loc : pt_result_cache.at(pt_res)) {
            ObjectLocator *revised_locator = loc->offsetBy(output_offset);
            revised_locator->storeValue(curr_output, callsite, 0);
          }
        }
      }
    }
  }
}

void IntraLotusAA::cacheFunctionCallInfo() {
  if (func_obj)
    return;

  func_obj = newObject(nullptr);
  ObjectLocator *loc = func_obj->findLocator(0, true);
  
  for (BasicBlock *bb : topBBs) {
    for (Instruction &inst : *bb) {
      if (CallBase *call = dyn_cast<CallBase>(&inst)) {
        if (Function *called = call->getCalledFunction()) {
          if (called->isIntrinsic())
            continue;
        }
        loc->storeValue(call, call, 0);
      }
    }
  }
}

void IntraLotusAA::computePTA() {
  if (is_considered_as_library || is_PTA_computed)
    return;

  // Cache instruction sequence
  int seq_num = 0;
  for (BasicBlock *bb : topBBs) {
    for (Instruction &inst : *bb) {
      value_seq[&inst] = seq_num++;
    }
  }

  cacheFunctionCallInfo();

  // Process instructions
  for (BasicBlock *bb : topBBs) {
    for (Instruction &inst : *bb) {
      switch (inst.getOpcode()) {
      case Instruction::Store:
        processStore(cast<StoreInst>(&inst));
        break;

      case Instruction::Load: {
        LoadInst *load = cast<LoadInst>(&inst);
        if (load->getType()->isPointerTy())
          processLoad(load);
        else {
          mem_value_t tmp;
          processBasePointer(load->getPointerOperand());
          loadPtrAt(load->getPointerOperand(), load, tmp, true);
        }
        break;
      }

      case Instruction::PHI:
        if (inst.getType()->isPointerTy())
          processPhi(cast<PHINode>(&inst));
        break;

      case Instruction::Alloca:
        processAlloca(cast<AllocaInst>(&inst));
        break;

      case Instruction::Call:
      case Instruction::Invoke:
        if (!isa<DbgInfoIntrinsic>(&inst)) {
          processCall(cast<CallBase>(&inst));
        }
        break;

      case Instruction::Select:
        if (inst.getType()->isPointerTy())
          processSelect(cast<SelectInst>(&inst));
        break;

      case Instruction::BitCast:
      case Instruction::GetElementPtr:
          processBasePointer(&inst);
        break;
      }
    }
  }

  // Collect interface
  if (IntraLotusAAConfig::lotus_restrict_inline_depth != 0) {
    collectOutputs();
    collectInputs();
    finalizeInterface();
  }

  is_PTA_computed = true;
}

void IntraLotusAA::show() {
  outs() << "\n========== LotusAA Results: " << analyzed_func->getName() << " ==========\n";
  
  // Show points-to sets
  for (auto &it : pt_results) {
    Value *ptr = it.first;
    if (!ptr)
      continue;
      
    PTResult *res = it.second;
    PTResultIterator iter(res, this);

    outs() << "Pointer: ";
    if (ptr->hasName())
      outs() << ptr->getName();
    else
      ptr->print(outs());
    outs() << " -> " << iter.size() << " locations\n";
    outs() << iter << "\n";
  }
  
  outs() << "==============================================\n\n";
}

// Utility and CG functions moved to separate files

