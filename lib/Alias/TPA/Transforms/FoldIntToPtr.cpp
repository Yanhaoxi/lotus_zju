#include "Alias/TPA/Transforms/FoldIntToPtr.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/PatternMatch.h>

using namespace llvm;
using namespace llvm::PatternMatch;

namespace transform {

static bool foldInstruction(IntToPtrInst *inst) {
  auto *op = inst->getOperand(0)->stripPointerCasts();

  // Pointer copy: Y = inttoptr (ptrtoint X)
  Value *src = nullptr;
  if (match(op, m_PtrToInt(m_Value(src)))) {
    // RAUW requires the replacement to have the exact same type.
    // `src` is a pointer, but its pointee type/address space can differ from
    // the `inttoptr` result type.
    if (src->getType() != inst->getType())
      src = CastInst::CreatePointerCast(src, inst->getType(), "src.cast", inst);
    inst->replaceAllUsesWith(src);
    inst->eraseFromParent();
    return true;
  }

  // Pointer arithmetic
  Value *offsetValue = nullptr;
  if (match(op, m_Add(m_PtrToInt(m_Value(src)), m_Value(offsetValue)))) {
    if (!offsetValue->getType()->isIntegerTy())
      return false;
    if (src->getType() != inst->getType())
      src = CastInst::CreatePointerCast(src, inst->getType(), "src.cast", inst);
    const auto &DL = inst->getModule()->getDataLayout();
    auto *indexTy = DL.getIndexType(src->getType());
    if (offsetValue->getType() != indexTy) {
      offsetValue =
          CastInst::CreateIntegerCast(offsetValue, indexTy,
                                      /*isSigned=*/true, "offset.cast", inst);
    }
    auto *gepInst =
        GetElementPtrInst::Create(inst->getType()->getPointerElementType(), src,
                                  {offsetValue}, inst->getName(), inst);
    inst->replaceAllUsesWith(gepInst);
    inst->eraseFromParent();
    return true;
  }

  return false;
}

PreservedAnalyses
FoldIntToPtrPass::run(llvm::Function &F,
                      FunctionAnalysisManager &analysisManager) {
  bool modified = false;
  for (auto &BB : F) {
    for (auto itr = BB.begin(); itr != BB.end();) {
      auto *inst = &*itr++;
      if (auto *itpInst = dyn_cast<IntToPtrInst>(inst))
        modified |= foldInstruction(itpInst);
    }
  }
  return modified ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

} // namespace transform
