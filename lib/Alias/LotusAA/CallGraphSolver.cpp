/*
 * LotusAA - Call Graph Resolution
 * 
 * Resolves indirect function calls using points-to information
 */

#include "Alias/LotusAA/IntraProceduralAnalysis.h"

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;
using namespace std;

static cl::opt<bool> lotus_print_cg_details(
    "lotus-print-cg-details",
    cl::desc("Print detailed CG resolution info"),
    cl::init(false), cl::Hidden);

void IntraLotusAA::resolveCallValue(Value *val, cg_result_t &target) {
  mem_value_t resolved_tmp;
  trackPtrRightValue(val, resolved_tmp);
  
  for (auto &item : resolved_tmp) {
    Value *resolved_val = item.val;
    
    if (Function *func = dyn_cast<Function>(resolved_val)) {
      // Direct function pointer
      target.insert(func);
    } else if (CallBase *call = dyn_cast<CallBase>(resolved_val)) {
      // Function returned from call
      Instruction *callsite_inst = call;
      
      // Get the callee's return value functions
      if (Function *called_func = call->getCalledFunction()) {
        IntraLotusAA *callee_PTG = lotus_aa->getPtGraph(called_func);
        if (callee_PTG && !callee_PTG->is_considered_as_library) {
          // Get output CG summary (index 0 = return value)
          if (!callee_PTG->output_cg_summary.empty()) {
            cg_result_t &out_values = callee_PTG->output_cg_summary[0];
            for (Function *func : out_values) {
              target.insert(func);
            }
          }
        }
      }
    } else if (Argument *resolved_arg = dyn_cast<Argument>(resolved_val)) {
      if (resolved_arg->getParent() || inputs.count(resolved_val)) {
        // Real argument or pseudo-argument
        map<cg_result_t *, bool> &in_summary = input_cg_summary[resolved_arg];
        in_summary[&target] = true;
      }
    }
  }
}

void IntraLotusAA::computeCG() {
  if (is_considered_as_library || !is_PTA_computed || is_CG_computed)
    return;

  Function *func = analyzed_func;

  // Resolve indirect calls
  for (BasicBlock *bb : topBBs) {
    for (Instruction &inst : *bb) {
      if (CallBase *call = dyn_cast<CallBase>(&inst)) {
        // Process both direct and indirect calls
        Function *base_func = func;
        auto callees = lotus_aa->getCallees(base_func, call);

        if (callees && IntraLotusAAConfig::lotus_restrict_inline_depth != 0) {
          // Inline input summaries from callees
          int callee_idx = 0;
          for (Function *callee : *callees) {
            if (callee_idx >= IntraLotusAAConfig::lotus_restrict_cg_size)
              break;

            if (!callee || lotus_aa->isBackEdge(base_func, callee)) {
              callee_idx++;
              continue;
            }

            if (!func_arg.count(call) || !func_arg[call].count(callee)) {
              callee_idx++;
              continue;
            }

            IntraLotusAA *callee_PTG = lotus_aa->getPtGraph(callee);
            if (callee_PTG && !callee_PTG->is_considered_as_library) {
              func_arg_t *caller_args = &func_arg[call][callee];

              // Process input CG summaries
              map<Argument *, map<cg_result_t *, bool>, llvm_cmp> &callee_input_cg_summary =
                  callee_PTG->input_cg_summary;

              for (auto &arg_summary_pair : callee_input_cg_summary) {
                Argument *callee_arg = arg_summary_pair.first;
                map<cg_result_t *, bool> &callee_arg_summary = arg_summary_pair.second;

                if (!caller_args->count(callee_arg))
                  continue;

                mem_value_t &caller_arg_values = (*caller_args)[callee_arg];

                for (auto &callee_summary_item : callee_arg_summary) {
                  cg_result_t *inline_target = callee_summary_item.first;
                  
                  for (auto &caller_arg_value : caller_arg_values) {
                    Value *caller_arg_value_val = caller_arg_value.val;
                    resolveCallValue(caller_arg_value_val, *inline_target);
                  }
                }
              }
            }
            callee_idx++;
          }
        }

        // Resolve the call value itself
        Value *called_value = call->getCalledOperand();
        resolveCallValue(called_value, cg_resolve_result[call]);
      }
    }
  }

  // Compute output CG summary
  if (IntraLotusAAConfig::lotus_restrict_inline_depth != 0) {
    int output_size = outputs.size();
    output_cg_summary.resize(output_size);
    
    for (int idx = 0; idx < output_size; idx++) {
      OutputItem *output_item = outputs[idx];
      if (!output_item->getType()->isPointerTy())
        continue;
      
      cg_result_t &target = output_cg_summary[idx];

      auto &src_value_all = output_item->getVal();
      for (auto &src_item : src_value_all) {
        mem_value_t &src = src_item.second;
        for (mem_value_item_t &value_item : src) {
          Value *src_value = value_item.val;
          resolveCallValue(src_value, target);
        }
      }
    }
  }

  is_CG_computed = true;
}

void IntraLotusAA::showFunctionPointers() {
  bool title_printed = false;
  
  for (auto &cg_item : cg_resolve_result) {
    Value *call_site = cg_item.first;
    if (CallBase *call = dyn_cast<CallBase>(call_site)) {
      if (call->getCalledFunction())
        continue;  // Skip direct calls
    }

    if (!title_printed) {
      outs() << "\n";
      outs() << "========== Function Pointers: " << analyzed_func->getName() << " ==========\n";
      title_printed = true;
    }

    outs() << "  Call Site: ";
    call_site->print(outs());
    outs() << "\n";
    
    cg_result_t &result = cg_item.second;
    for (Function *resolved_func : result) {
      outs() << "    -> " << resolved_func->getName() << "\n";
    }
  }

  if (title_printed)
    outs() << "===============================================\n\n";
}

