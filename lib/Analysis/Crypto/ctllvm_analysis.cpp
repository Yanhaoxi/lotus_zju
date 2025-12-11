/*
 * Core taint/alias analysis for the ctllvm pass.
 */

#include "Analysis/Crypto/ctllvm.h"

#include "llvm/Analysis/MemoryLocation.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"

#include <cassert>
#include <fstream>

using namespace llvm;

/* USER INPUT HERE */
bool CTPass::update_target_values(
    llvm::SetVector<target_value_info *> &target_values,
    llvm::SetVector<target_value_info *> &declassified_values) {
  struct target_value_info *target1 = new target_value_info();
  target1->function_name = "mpi_powm";
  target1->value_name = "exponent";
  target1->value_type = "gcry_mpi";
  target1->field_name = "d";
  target_values.insert(target1);

  struct target_value_info *target4 = new target_value_info();
  target4->function_name = "AES_ige_encrypt";
  target4->value_name = "in";
  target_values.insert(target4);

  struct target_value_info *target2 = new target_value_info();
  target2->function_name = "ec_GF2m_montgomery_point_multiply";
  target2->value_name = "scalar";
  target2->value_type = "bignum_st";
  target2->field_name = "d";
  target_values.insert(target2);

  struct target_value_info *target3 = new target_value_info();
  target3->function_name = "ec_wNAF_mul";
  target3->value_name = "wNAF";
  target_values.insert(target3);

  return target_values.size() > 0 || declassified_values.size() > 0;
}

int CTPass::Analyze_Function(Function &F, FunctionAnalysisManager &FAM) {
#if DEBUG
  errs() << "!!!!!!!!!!Start Analyzing: " << F.getName() << "!!!!!!!!!!\n";
#endif

  int analysis_result = 0;
  int violation_count = 0;
  auto &AA = FAM.getResult<AAManager>(F);
  Module &M = *F.getParent();

  llvm::SetVector<Instruction *> SorLInstructions, SelectInstructions;
  llvm::SetVector<Value *> tainted_values, declassified_values;
  llvm::SetVector<Function *> untracked_functions;

#if PRINT_FUNCTION
  F.print(errs());
#endif

  // Go through dbg metadata and update the tainted_values
  for (auto &BB : F) {
    for (auto &I : BB) {
      if (llvm::isa<llvm::LoadInst>(I) || llvm::isa<llvm::StoreInst>(I))
        SorLInstructions.insert(&I);
      if (llvm::isa<llvm::MemCpyInst>(I) || llvm::isa<llvm::MemMoveInst>(I)) {
#if SOUNDNESS_MODE
        // abort if the size is not a constant
        if (!llvm::isa<llvm::ConstantInt>(I.getOperand(2)))
          return ERROR_CODE_NO_CONSTANT_SIZE;
#endif
        SorLInstructions.insert(&I);
      }
      if (llvm::isa<SelectInst>(I))
        SelectInstructions.insert(&I);

      if (!specify_taint_flag)
        continue;
      /* It is possible a specified value is not found in Debug Info for
        following reasons: 1) The value is optimized out 2) The specified value
        name etc. is incorrect */
      update_taint_list(M, F, I, false, tainted_values, specify_target_values);
      update_taint_list(M, F, I, true, declassified_values,
                        specify_declassified_values);
    }
  }

#ifdef ALIAS_THRESHOLD
  if (SorLInstructions.size() > ALIAS_THRESHOLD) {
    return ERROR_CODE_TOO_MANY_ALIAS;
  }
#endif

#if TEST_PARAMETER
  for (auto &arg : F.args())
    tainted_values.insert(&arg);
#endif

#if DEBUG
  errs() << "<--Initial Taint Values and Declassified Values START-->\n";
  for (auto &val : tainted_values)
    errs() << "[INFO.Inital] Tainted Value: " << *val << " at line "
           << getDebugInfo<int>(val, "", F) << "\n";
  for (auto &val : declassified_values)
    errs() << "[INFO.Inital] Declassified Value: " << *val << " at line "
           << getDebugInfo<int>(val, "", F) << "\n";
  errs() << "<--Initial Taint Values and Declassified Values DONE-->\n";
#endif

  for (auto &Arg : tainted_values) {

    statistics_taint_source++;

    high_values.clear();
    low_values.clear();
    high_mayvalues.clear();
    low_mayvalues.clear();

#if DEBUG
    errs() << "**********Analyzing Taint Source: " << *Arg << "**********\n";
#endif

    llvm::SetVector<Instruction *> taintedInstructions, aliasedInstructions;
    std::map<Instruction *, int> leak_through_cache, leak_through_branch,
        leak_through_variable_timing;
    std::map<Instruction *, int> may_leak_through_cache,
        may_leak_through_branch, may_leak_through_variable_timing;

    if (check_pointer_type(Arg->getType()))
      low_values.insert(Arg);
    else
      high_values.insert(Arg);

    // Get all direct users of the argument
    for (auto *U : Arg->users()) {
      if (Instruction *Inst = dyn_cast<Instruction>(U)) {
        wrap_metadata(*Inst, dyn_cast<Value>(Arg), false, true);
        taintedInstructions.insert(Inst);

#if DEBUG
        int debug_line = getDebugInfo<int>(Inst, "", F);
        errs() << "[DEFUSE.Add] " << *Inst << " at line " << debug_line << "\n";
#endif
      }
    }
#if DEBUG
    errs() << "============Done Initial Tainting============\n";
#endif

    int local_analysis_result = 0;
    def_use_only(taintedInstructions, declassified_values);
    def_use_alias(taintedInstructions, aliasedInstructions, SorLInstructions,
                  AA, Arg, declassified_values);
    violation_count =
        check_and_report(Arg, F, FAM, taintedInstructions, leak_through_cache,
                         leak_through_branch, leak_through_variable_timing,
                         1); // report must leaks
    local_analysis_result =
        local_analysis_result << 1 | (violation_count > 0 ? 1 : 0);

#if ENABLE_MAY_LEAK || SOUNDNESS_MODE
    // Merge high_mayvalues and low_mayvalues to high_values and low_values
    for (auto &val : high_mayvalues)
      high_values.insert(val);
    for (auto &val : low_mayvalues)
      low_values.insert(val);

    def_use_may_alias(taintedInstructions, aliasedInstructions,
                      SorLInstructions, AA, Arg, declassified_values);
    violation_count = check_and_report(
        Arg, F, FAM, aliasedInstructions, may_leak_through_cache,
        may_leak_through_branch, may_leak_through_variable_timing,
        2); // report may leaks
    local_analysis_result =
        local_analysis_result << 2 | (violation_count > 0 ? 1 : 0);
#endif

    if (local_analysis_result == 0)
      statistics_secure_taint_source++;

    analysis_result |= local_analysis_result;

    taintedInstructions.clear();
    aliasedInstructions.clear();
    leak_through_cache.clear();
    leak_through_branch.clear();
    leak_through_variable_timing.clear();
    may_leak_through_cache.clear();
    may_leak_through_branch.clear();
    may_leak_through_variable_timing.clear();
  }

  statistics_analyzed_functions++;
  if (analysis_result == 0)
    statistics_secure_functions++;

#if SOUNDNESS_MODE
  std::string analysis_string =
      analysis_result == 0 ? "proved-CT" : "proved-NCT";
  errs() << F.getName() << " is: " << analysis_string << "\n";
#endif
  return analysis_result;
}

void CTPass::def_use_only(llvm::SetVector<Instruction *> &taintedInstructions,
                          llvm::SetVector<Value *> &declassified_values) {
  build_dependency_chain(taintedInstructions, declassified_values);
}

// The function can only be called when def_use_only is called
void CTPass::def_use_alias(llvm::SetVector<Instruction *> &taintedInstructions,
                           llvm::SetVector<Instruction *> &aliasedInstructions,
                           llvm::SetVector<Instruction *> &SorLInstructions,
                           AAResults &AA, Value *Arg,
                           llvm::SetVector<Value *> &declassified_values) {
  int prev_num = taintedInstructions.size();
  int new_num = -1;
  while (prev_num != new_num) {
    new_num = prev_num;
    find_aliased_instructions(aliasedInstructions, taintedInstructions,
                              SorLInstructions, AA, Arg, declassified_values);
    if (prev_num == taintedInstructions.size())
      break; // optimize
    prev_num = build_dependency_chain(taintedInstructions, declassified_values);
  }
}

// The function can only be called when def_use_alias is called
void CTPass::def_use_may_alias(
    llvm::SetVector<Instruction *> &taintedInstructions,
    llvm::SetVector<Instruction *> &aliasedInstructions,
    llvm::SetVector<Instruction *> &SorLInstructions, AAResults &AA, Value *Arg,
    llvm::SetVector<Value *> &declassified_values) {

  // Now we update the aliasedInstructions, and we should have an initial list
  // of aliasedInstructions
  llvm::SetVector<Instruction *> subaliasedInstructions;
  int prev_num_aliased =
      build_dependency_chain(aliasedInstructions, declassified_values);
  int new_num_aliased = -1;
  while (prev_num_aliased != new_num_aliased) {
    new_num_aliased = prev_num_aliased;
    find_aliased_instructions(subaliasedInstructions, aliasedInstructions,
                              SorLInstructions, AA, Arg, declassified_values);

    // merge high_mayvalues and low_mayvalues to high_values and low_values
    for (auto &val : high_mayvalues)
      high_values.insert(val);
    for (auto &val : low_mayvalues)
      low_values.insert(val);

    if (subaliasedInstructions.size() == 0 &&
        prev_num_aliased == aliasedInstructions.size())
      break;
    for (auto &I : subaliasedInstructions)
      aliasedInstructions.insert(I);
    prev_num_aliased =
        build_dependency_chain(aliasedInstructions, declassified_values);
  }
}

int CTPass::check_and_report(
    Value *Arg, Function &F, FunctionAnalysisManager &FAM,
    llvm::SetVector<Instruction *> &taintedInstructions,
    std::map<Instruction *, int> &leak_through_cache,
    std::map<Instruction *, int> &leak_through_branch,
    std::map<Instruction *, int> &leak_through_variable_timing, int mode) {

  checkInstructionLeaks(taintedInstructions, leak_through_cache,
                        leak_through_branch, leak_through_variable_timing, Arg,
                        F, FAM);

  int tainted_line = getDebugInfo<int>(Arg, "", F);
  int analyzed_lines = taintedInstructions.size();
  StringRef tested_value_name = getDebugInfo<StringRef>(Arg, "", F);
  StringRef file_name = "";
  if (DISubprogram *SP = F.getSubprogram())
    file_name = SP->getFilename();
  else
    file_name = F.getParent()->getSourceFileName();

    // It is a trick, the tainted value could be stored in to stack and thus we
    // cannot trace back So let's get the store operation in its direct use and
    // then use the store address to trace back
    // TODO: Do we have a more elegant way to do this?
#if TRY_HARD_ON_NAME
  if (tainted_line == -1 && tested_value_name == "") {
    for (auto *U : Arg->users()) {
      if (auto *SI = dyn_cast<StoreInst>(U)) {
        Value *store_address = SI->getPointerOperand();
        tainted_line = getDebugInfo<int>(store_address, "", F);
        tested_value_name = getDebugInfo<StringRef>(store_address, "", F);
        break;
      }
    }
  }
#endif

  StringRef FunctionName = F.getName();
  if (FUNC_NAME_ENDS_WITH(FunctionName, "_ctcloned"))
    FunctionName = FunctionName.drop_back(9);
  errs() << "{"
         << "\"function\": \"" << FunctionName << "\", "
         << "\"file\": \"" << file_name << "\", "
         << "\"tested_value\": \"" << tested_value_name << "\", "
         << "\"line\": \"" << tainted_line << "\", "
         << "\"IR\": \"" << *Arg << "\", "
         << "\"LoCs\": " << analyzed_lines << ", "
         << "\"Feature\": " << mode << ", "
         << "\"cache\": " << leak_through_cache.size() << ", "
         << "\"branch\": " << leak_through_branch.size() << ", "
         << "\"vt\": " << leak_through_variable_timing.size() << "}\n";
#if REPORT_LEAKAGES
  report_leakage(taintedInstructions, leak_through_cache, leak_through_branch,
                 leak_through_variable_timing, mode);
#endif
  return leak_through_cache.size() + leak_through_branch.size() +
         leak_through_variable_timing.size();
}

int CTPass::build_dependency_chain(
    llvm::SetVector<Instruction *> &taintedInstructions,
    llvm::SetVector<Value *> &declassified_values) {
  SetVector<Instruction *> worklist(taintedInstructions.begin(),
                                    taintedInstructions.end());

  while (!worklist.empty()) {
    Instruction *I = worklist.pop_back_val();
#if DEBUG
    std::string label =
        high_values.contains(dyn_cast<Value>(I)) ? "high" : "low";
    errs() << "[DEFUSE.Start] " << *I << " " << label << " at line "
           << getDebugInfo<int>(I, "", *I->getFunction()) << "\n";
#endif
    bool declassified_flag =
        (isa<Value>(I) && declassified_values.contains(dyn_cast<Value>(I)));

    for (auto *U : I->users()) {
      if (auto *Inst = dyn_cast<Instruction>(U)) {
        if (declassified_flag) {
#if DEBUG
          errs() << "[DEFUSE.DECLASSIFIED] " << *Inst << " at line "
                 << getDebugInfo<int>(Inst, "", *Inst->getFunction()) << "\n";
#endif
          continue;
        }

        bool reevaluate_flag = wrap_metadata(*Inst, dyn_cast<Value>(I), false);
        bool insertResult = taintedInstructions.insert(Inst) || reevaluate_flag;
        if (insertResult)
          worklist.insert(Inst);

#if DEBUG
        if (insertResult) {
          std::string label =
              high_values.contains(dyn_cast<Value>(Inst)) ? "high" : "low";
          errs() << "[DEFUSE.Add] " << *Inst << " " << label << " at line "
                 << getDebugInfo<int>(Inst, "", *Inst->getFunction()) << "\n";
        }
#endif
      }
    }
  }

  return taintedInstructions.size();
}

int CTPass::find_aliased_instructions(
    llvm::SetVector<Instruction *> &aliasedInstructions,
    llvm::SetVector<Instruction *> &taintedInstructions,
    llvm::SetVector<Instruction *> &SorLInstructions, AAResults &AA, Value *Arg,
    llvm::SetVector<Value *> &declassified_values) {
  llvm::SetVector<Instruction *> taintedInstructions_temp,
      aliasedInstructions_temp;

  for (auto &I : taintedInstructions) {
    bool high_in_memcpy = false;
    Value *stored_value = nullptr;
    MemoryLocation SI_loc; // Store Address Location
    uint64_t memcopy_size = 0;
    bool memcpy_flag = false;
    if (isa<StoreInst>(I)) {
      // Check if the stored value is tainted or is the argument
      stored_value = cast<StoreInst>(I)->getValueOperand();
      if (!taintedInstructions.contains(dyn_cast<Instruction>(stored_value)) &&
          (stored_value != Arg))
        continue;
      if (declassified_values.contains(dyn_cast<Value>(stored_value)))
        continue;
      SI_loc = MemoryLocation::get(cast<StoreInst>(I));
    } else if (isa<MemCpyInst>(I) || isa<MemMoveInst>(I)) {
      if (isa<MemCpyInst>(I))
        SI_loc = MemoryLocation::getForDest(cast<MemCpyInst>(I));
      else
        SI_loc = MemoryLocation::getForDest(cast<MemMoveInst>(I));
      memcpy_flag = true;
      Value *copy_size = I->getOperand(2);
      if (auto *const_size = dyn_cast<ConstantInt>(copy_size)) {
        memcopy_size = const_size->getZExtValue();
      }

      // If the source address is tainted, we check whether it is high
      // If high, it already leaks through memory access
      // If low, it means it is propagated from a taint source
      if (auto *src = dyn_cast<Value>(I->getOperand(1))) {
        if (taintedInstructions.contains(dyn_cast<Instruction>(src)) ||
            src == Arg) {
          if (high_values.contains(src))
            high_in_memcpy = false;
          else
            high_in_memcpy = true;
        }
      } else if (!memcopy_size)
        high_in_memcpy = true;
      else
        high_in_memcpy = reason_memcpy(*I, AA, SorLInstructions);
    } else
      continue;

    for (auto &J : SorLInstructions) {
      if (taintedInstructions.contains(J) &&
          high_values.contains(dyn_cast<Value>(J)))
        continue;
      MemoryLocation LI_loc; // Load Address Location
      if (auto *LI = dyn_cast<LoadInst>(J)) {
        LI_loc = MemoryLocation::get(LI);
      } else if (auto *LI = dyn_cast<MemCpyInst>(J)) {
        LI_loc = MemoryLocation::getForSource(LI);
      } else
        continue;

      auto *LI = dyn_cast<Instruction>(J);
      if (!isPotentiallyReachable(I, LI, nullptr, nullptr))
        continue;

      AliasResult AR = AA.alias(SI_loc, LI_loc);
      bool may_alias_memcpy = false;
      if (memcpy_flag) {
        for (uint64_t i = 0; i < memcopy_size; i++) {
          if (AR == AliasResult::MustAlias || AR == AliasResult::PartialAlias)
            break;
          if (AR == AliasResult::MayAlias)
            may_alias_memcpy = true;
          MemoryLocation new_SI_loc(SI_loc.Ptr, i);
          AR = AA.alias(new_SI_loc, LI_loc);
        }
      }

      if (memcpy_flag &&
          (AR != AliasResult::MustAlias && AR != AliasResult::PartialAlias) &&
          may_alias_memcpy)
        AR = AliasResult::MayAlias;

#if DEBUG
      if (AR != AliasResult::NoAlias)
        errs() << "[Alias.Result] " << AR << " " << *I << " and " << *J << "\n";
      int debug_line = LI->getDebugLoc() ? LI->getDebugLoc().getLine() : -1;
#endif
      if (AR == AliasResult::NoAlias)
        continue;

      llvm::SetVector<Value *> &high_values_ptr =
          (AR == AliasResult::MustAlias || AR == AliasResult::PartialAlias)
              ? high_values
              : high_mayvalues;
      llvm::SetVector<Value *> &low_values_ptr =
          (AR == AliasResult::MustAlias || AR == AliasResult::PartialAlias)
              ? low_values
              : low_mayvalues;

      if (isa<StoreInst>(I) && !J->getType()->isVoidTy()) {
        if (high_values.contains(stored_value))
          high_values_ptr.insert(dyn_cast<Value>(LI));
        else
          low_values_ptr.insert(dyn_cast<Value>(LI));
      } else if ((isa<MemCpyInst>(I) || isa<MemMoveInst>(I)) &&
                 !J->getType()->isVoidTy()) {
        if (high_in_memcpy)
          high_values_ptr.insert(dyn_cast<Value>(LI));
        else
          low_values_ptr.insert(dyn_cast<Value>(LI));
      }

      if (AR == AliasResult::MustAlias || AR == AliasResult::PartialAlias) {
#if DEBUG
        std::string label = (high_values.contains(dyn_cast<Value>(LI)) |
                             high_values_ptr.contains(dyn_cast<Value>(LI)))
                                ? "high"
                                : "low";
        errs() << "[Alias.Must.Add] " << *J << " " << label << " at line "
               << debug_line << "\n";
#endif
        taintedInstructions_temp.insert(LI);
      } else {
#if DEBUG
        std::string label = (high_values.contains(dyn_cast<Value>(LI)) |
                             high_values_ptr.contains(dyn_cast<Value>(LI)))
                                ? "high"
                                : "low";
        errs() << "[Alias.May.Add] " << *J << " " << label << " at line "
               << debug_line << "\n";
#endif
        aliasedInstructions_temp.insert(LI);
      }
    }
  }

  for (auto &I : taintedInstructions_temp)
    taintedInstructions.insert(I);
  for (auto &I : aliasedInstructions_temp)
    aliasedInstructions.insert(I);

  taintedInstructions_temp.clear();
  aliasedInstructions_temp.clear();

  return aliasedInstructions.size();
}

// I has to be a memcpy or memmove instruction
bool CTPass::reason_memcpy(llvm::Instruction &I, AliasAnalysis &AA,
                           llvm::SetVector<Instruction *> &SorLInstructions) {
  if (!llvm::isa<llvm::MemCpyInst>(I) && !llvm::isa<llvm::MemMoveInst>(I))
    return false;
  // Get source location
  MemoryLocation memcpy_src_loc;
  if (auto *LI = dyn_cast<MemCpyInst>(&I)) {
    memcpy_src_loc = MemoryLocation::getForSource(LI);
  } else if (auto *LI = dyn_cast<MemMoveInst>(&I)) {
    memcpy_src_loc = MemoryLocation::getForSource(LI);
  }

  // Get the size of copied memory
  uint64_t memcopy_size = 0;
  Value *copy_size = I.getOperand(2);
  if (auto *const_size = dyn_cast<ConstantInt>(copy_size)) {
    memcopy_size = const_size->getZExtValue();
  }
  assert(memcopy_size != 0 && "Memcopy size is zero");

  // Now, we check whether there is a store that stores an address aliased with
  // src Wee need to use the alias analysis to check this
  for (auto &J : SorLInstructions) {
    auto *SI = dyn_cast<StoreInst>(J);

    if (!SI || !isPotentiallyReachable(J, &I, nullptr, nullptr))
      continue;

    // check if the stored value is HIGH
    auto *stored_value = SI->getValueOperand();
    if (!stored_value || !high_values.contains(stored_value))
      continue;

    // Check if the store address is aliased with src
    MemoryLocation store_loc = MemoryLocation::get(SI);
    AliasResult AR = AA.alias(memcpy_src_loc, store_loc);

    // Compute new location by iterating over the memory copy size and check if
    // the store location is aliased with the new location
    bool may_alias_memcpy = false;
    for (uint64_t i = 0; i < memcopy_size; i++) {
      if (AR == AliasResult::MustAlias || AR == AliasResult::PartialAlias)
        return true;
      if (AR == AliasResult::MayAlias)
        may_alias_memcpy = true;
      MemoryLocation new_memcpy_src_loc(memcpy_src_loc.Ptr, i);
      AR = AA.alias(new_memcpy_src_loc, store_loc);
    }
#if ENABLE_MAY_LEAK
    if (may_alias_memcpy)
      return true;
#endif
  }
  return false;
}

void CTPass::checkInstructionLeaks(
    llvm::SetVector<Instruction *> &taintedInstructions,
    std::map<Instruction *, int> &leak_through_cache,
    std::map<Instruction *, int> &leak_through_branch,
    std::map<Instruction *, int> &leak_through_variable_timing, Value *Arg,
    Function &F, FunctionAnalysisManager &FAM) {
  for (auto &taintedInstr : taintedInstructions) {
    int line_number = taintedInstr->getDebugLoc()
                          ? taintedInstr->getDebugLoc().getLine()
                          : -1;
    if (llvm::isa<llvm::BranchInst>(taintedInstr) ||
        llvm::isa<llvm::SwitchInst>(taintedInstr) ||
        llvm::isa<llvm::SelectInst>(taintedInstr)) {
      Value *Cond = nullptr;
      if (llvm::isa<llvm::BranchInst>(taintedInstr)) {
        Cond = llvm::cast<llvm::BranchInst>(taintedInstr)->getCondition();
      } else if (llvm::isa<llvm::SwitchInst>(taintedInstr)) {
        Cond = llvm::cast<llvm::SwitchInst>(taintedInstr)->getCondition();
      } else if (llvm::isa<llvm::SelectInst>(taintedInstr)) {
        Cond = llvm::cast<llvm::SelectInst>(taintedInstr)->getCondition();
      }

#if TYPE_SYSTEM
      if (Cond && high_values.contains(Cond))
        leak_through_branch[taintedInstr] = line_number;
#else
      if (Cond && taintedInstructions.contains(dyn_cast<Instruction>(Cond)))
        leak_through_branch[taintedInstr] = line_number;
#endif
      continue;
    }

    // Check if variable-timing instructions leaks.
    // Now only check division instructions
    if (llvm::isa<llvm::BinaryOperator>(taintedInstr)) {
      llvm::BinaryOperator *BO = dyn_cast<BinaryOperator>(taintedInstr);
      if (BO->getOpcode() == llvm::Instruction::SDiv ||
          BO->getOpcode() == llvm::Instruction::UDiv) {
#if TYPE_SYSTEM
        // High type should already be propagated to the defined SSA value
        if (BO && high_values.contains(BO))
          leak_through_variable_timing[taintedInstr] = line_number;
#else
        if (BO && taintedInstructions.contains(BO))
          leak_through_variable_timing[taintedInstr] = line_number;
#endif
      }
      continue;
    }

    // Check if cache instruction leaks
    if (llvm::isa<llvm::LoadInst>(taintedInstr) ||
        llvm::isa<llvm::StoreInst>(taintedInstr)) {
      llvm::Value *Ptr = nullptr;
      if (llvm::isa<llvm::LoadInst>(taintedInstr)) {
        Ptr = llvm::cast<llvm::LoadInst>(taintedInstr)->getPointerOperand();
      } else if (llvm::isa<llvm::StoreInst>(taintedInstr)) {
        Ptr = llvm::cast<llvm::StoreInst>(taintedInstr)->getPointerOperand();
      }
#if TYPE_SYSTEM
      if (Ptr && high_values.contains(Ptr))
        leak_through_cache[taintedInstr] = line_number;
#else
      if (Ptr && taintedInstructions.contains(dyn_cast<Instruction>(Ptr)))
        leak_through_cache[taintedInstr] = line_number;
#endif
      continue;
    }
  }
}

void CTPass::printLeakage(const std::string &type,
                          const std::map<Instruction *, int> &leakMap,
                          int may_must,
                          llvm::SetVector<Instruction *> &taintedInstructions) {
  for (const auto &it : leakMap) {
    StringRef filename = "unknown";
    std::string localType = type;
    // check if it is a select instruction, if so store select into type
    if (llvm::isa<llvm::SelectInst>(it.first))
      localType = "select";

    if (it.second != -1) {
      filename = it.first->getModule()->getSourceFileName();
      if (auto dbgLoc = it.first->getDebugLoc()) {
        auto *scope = dbgLoc->getScope();
        if (scope)
          filename = scope->getFilename();
      }
    }

#if DEBUG
    if (may_must != 2)
      errs() << localType << " violate CT policy at: " << *it.first << " in "
             << FILE_PATH + filename.str() << " at line " << it.second << "\n";
    else
      errs() << "May " << localType << " violate CT policy at: " << *it.first
             << " in " << FILE_PATH + filename.str() << " at line "
             << " at line " << it.second << "\n";
#else
    if (may_must != 2)
      errs() << "  Violate CT policy: " << localType << " in file "
             << FILE_PATH + filename.str() << " at line " << it.second << "\n";
    else
      errs() << "  May Violate CT policy: " << localType << " in file "
             << FILE_PATH + filename.str() << " at line " << it.second << "\n";
#endif

    if (it.second != -1)
      print_source_code(filename.str(), it.second);
  }
}

void CTPass::report_leakage(
    llvm::SetVector<Instruction *> &taintedInstructions,
    std::map<Instruction *, int> &leak_through_cache,
    std::map<Instruction *, int> &leak_through_branch,
    std::map<Instruction *, int> &leak_through_variable_timing, int may_must) {
  bool has_leakage =
      (!leak_through_cache.empty() || !leak_through_branch.empty() ||
       !leak_through_variable_timing.empty());
  if (!has_leakage)
    return;

  // errs().changeColor(raw_ostream::RED);
  printLeakage("cache", leak_through_cache, may_must, taintedInstructions);
  printLeakage("branch", leak_through_branch, may_must, taintedInstructions);
  printLeakage("variable timing", leak_through_variable_timing, may_must,
               taintedInstructions);
  // errs().resetColor();
}

void CTPass::print_source_code(std::string filename, int line_number) {
  filename = FILE_PATH + filename;
  std::ifstream file(filename);
  if (!file.is_open()) {
    errs() << "Cannot open file " << filename << "\n";
    return;
  }
  errs().changeColor(raw_ostream::RED); //MAGENTA
  std::string source_line;
  // print source code at the line
  for (unsigned CurrentLine = 1; std::getline(file, source_line);
       ++CurrentLine) {
    if (CurrentLine == line_number) {
      errs() << "  "
             << "-->" << source_line << "\n";
      break;
    }
  }
  // errs().changeColor(raw_ostream::RED);
  errs().resetColor();
  file.close();
}

// @arg is the previous value
bool CTPass::wrap_metadata(llvm::Instruction &I, Value *Arg, bool alias_flag,
                           bool init_flag, Value *init_taint_value) {

#if !TYPE_SYSTEM
  return false;
#endif

  int already_high = high_values.contains(&I) ? 1 : 0;

  // Do not type an instruction that do not define new SSA Value
  if (I.getType()->isVoidTy())
    goto type_leave;

  // If it is already high, we do not change it
  if (high_values.contains(&I))
    goto type_leave;

  // Initial case
  if (init_flag) {
    // If Arg is pointer, we check whether it loads a non-pointer value,
    // Otherwise, we set it to Low
    if (check_pointer_type(Arg->getType())) {
      if (llvm::isa<llvm::LoadInst>(I)) {
        Value *loaded_value = dyn_cast<Value>(&I);
        if (check_pointer_type(loaded_value->getType()))
          low_values.insert(&I); // Loaded another pointer, it is low
        else
          high_values.insert(&I); // Loaded a non-pointer, it is high
      } else {
        low_values.insert(&I); // Any computation, except load, on ptr is low
      }
    } else {
      high_values.insert(&I); // Arg is not a pointer, any operation is high
    }
    goto type_leave;
  }

  // Any loaded non-pointer value is high, otherwise, it is low
  if (llvm::isa<llvm::LoadInst>(I)) {
    Value *loaded_value = dyn_cast<Value>(&I);
    if (!check_pointer_type(loaded_value->getType())) {
      high_values.insert(&I);
    } else {
      low_values.insert(&I);
    }
    goto type_leave;
  }

  // If high than anything is high
  if (high_values.contains(Arg)) {
    high_values.insert(&I);
  } else {
    low_values.insert(&I);
  }

  // Only alias analysis can make a loaded pointer high
  if (isa<LoadInst>(I)) {
    if (!check_pointer_type(I.getType()))
      high_values.insert(&I);
    else
      low_values.insert(&I);
    goto type_leave;
  }

type_leave:
  int now_high = high_values.contains(&I) ? 1 : 0;
  bool re_evaluate = now_high != already_high;
  return re_evaluate;
}

int CTPass::getFieldIndex(StructType *StructTy, StringRef FieldName,
                          const Module &M) {
  DebugInfoFinder Finder;
  Finder.processModule(M);
  StringRef Structure_name = StructTy->getName().drop_front(7);
  for (auto *Type : Finder.types()) {
    if (auto *Composite = dyn_cast<DICompositeType>(Type)) {
      if (Composite->getName() != Structure_name)
        continue;
      unsigned index = 0;
      for (const auto *Element : Composite->getElements()) {
        if (auto *Member = dyn_cast<DIDerivedType>(Element)) {
          if (FieldName == Member->getName())
            return index;
          index += 1;
        }
      }
    }
  }
  return -1;
}

bool CTPass::update_taint_list(
    Module &M, Function &F, llvm::Instruction &I, bool declassify_flag,
    llvm::SetVector<Value *> &tainted_values,
    const llvm::SetVector<target_value_info *> &entries) {
  bool handled_structure = false, handled_variable = false;
  // To handle structure, we target at the loaded value.
  if (auto *GEP = dyn_cast<GetElementPtrInst>(&I)) {
    if (auto *StructTy = dyn_cast<StructType>(GEP->getSourceElementType())) {
      for (const auto &target_value : entries) {
        if (target_value->function_name != F.getName())
          continue; // Not the target function
        if (target_value->field_name == "0")
          continue; // Indicating it is not a structure
        if (target_value->value_type != StructTy->getName().drop_front(7))
          continue; // Indicating not matched structure type
        if (auto *FieldIndex = dyn_cast<ConstantInt>(GEP->getOperand(2))) {
          if (FieldIndex->getZExtValue() ==
              getFieldIndex(StructTy, target_value->field_name, M)) {
            // Get the source value of the GEP instruction and get its name
            Value *source_value = GEP->getPointerOperand();
            StringRef tmp_name = getDebugInfo<StringRef>(source_value, "", F);
            if (tmp_name != target_value->value_name)
              continue;
            if (declassify_flag)
              if (getDebugInfo<int>(source_value, "", F) !=
                  target_value->line_number)
                continue;
#if DEBUG
            errs() << "[FOUND.Structure] " << tmp_name << "\n";
#endif
            tainted_values.insert(dyn_cast<Value>(GEP));
            handled_structure = true;
          }
        }
      }
    }
  }

  if (handled_structure)
    return true;

  DILocalVariable *LocalVar = nullptr;
  Value *Arg = nullptr;

#if USE_NEW_DEBUG_INFO
  // Handle both old and new debug info formats
  for (DbgRecord &DR : I.getDbgRecordRange()) {
    if (auto *Dbg = dyn_cast<DbgVariableRecord>(&DR)) {
      LocalVar = Dbg->getVariable();
      Arg = Dbg->getValue();
    }
  }
#endif

  if (!LocalVar) {
    if (auto *DbgDeclare = dyn_cast<DbgDeclareInst>(&I)) {
      LocalVar = DbgDeclare->getVariable();
      Arg = DbgDeclare->getAddress();
    } else if (auto *DbgValue = dyn_cast<DbgValueInst>(&I)) {
      LocalVar = DbgValue->getVariable();
      Arg = DbgValue->getValue();
    }
  }

  if (!LocalVar)
    return false;

  // Handle variables
  for (const auto &target_value : entries) {
    if (target_value->function_name != F.getName())
      continue;
    if (target_value->field_name != "0")
      continue;
    if (target_value->value_name != LocalVar->getName())
      continue;
    if (declassify_flag)
      if (getDebugInfo<int>(Arg, "", F) != target_value->line_number)
        continue;
#if DEBUG
    errs() << "[FOUND.Variable] " << LocalVar->getName() << "\n";
#endif
    tainted_values.insert(Arg);
    handled_variable = true;
  }

  return handled_variable;
}
