#include "Verification/SymbolicAbstraction/Core/ValueMapping.h"
#include "Verification/SymbolicAbstraction/Core/FloatingPointModel.h"
#include "Verification/SymbolicAbstraction/Core/MemoryModel.h"

#include <sstream>

namespace symbolic_abstraction
{
z3::expr ValueMapping::operator[](llvm::Value* value) const
{
    z3::expr res = this->getFullRepresentation(value);
    if (value->getType()->isPointerTy())
        return FunctionContext_.getMemoryModel().get_ptr_value(res);
    else
        return res;
}

z3::expr ValueMapping::getFullRepresentation(llvm::Value* value) const
{
    bool primed;

    if (AtBeginning_) {
        primed = false;
    } else if (AtEnd_) {
        primed = true;
    } else if (Fragment_.defines(value)) {
        auto *inst = llvm::cast<llvm::Instruction>(value);
        primed = (inst != Point_ && Fragment_.reachable(inst, Point_));
    } else {
        primed = false;
    }

    // form appropriate name for primed/unprimed var
    std::string name = value->getName().str();
    if (!llvm::isa<llvm::Argument>(value)) {
        if (primed)
            name = name + "_1";
        else
            name = name + "_0";
    }

    llvm::Type* type = value->getType();
    return FunctionContext_.getZ3().constant(
        name.c_str(), FunctionContext_.sortForType(type));
}

z3::expr ValueMapping::memory() const
{
    llvm::BasicBlock* bb;
    int mem_ops = 0;

    if (Point_ != nullptr) {
        bb = Point_->getParent();
    } else if (AtEnd_) {
        bb = Fragment_.getEnd();
    } else {
        bb = Fragment_.getStart();
    }

    if (bb != nullptr && (Point_ != nullptr || (AtEnd_ && IncludesEndBody_))) {
        for (auto& inst : *bb) {
            if (&inst == Point_)
                break; // never happens if AtEnd_

            // New memory variable is required after alloca, store or call
            // instruction. Transfer formulas that relate these variables are
            // generated in InstructionSemantics (for loads and stores; in case
            // of calls it's just a fresh variable since we cannot assume
            // anything).
            if (llvm::isa<llvm::StoreInst>(inst) ||
                llvm::isa<llvm::AllocaInst>(inst) ||
                llvm::isa<llvm::CallInst>(inst)) {
                ++mem_ops;
            }
        }
    }

    std::ostringstream name;
    name << "mem_";

    if (bb == Fragment::EXIT)
        name << "EXIT";
    else
        name << bb->getName().str();

    name << "_" << mem_ops;

    auto sort = FunctionContext_.getMemoryModel().sort();
    return FunctionContext_.getZ3().constant(name.str().c_str(), sort);
}

ValueMapping ValueMapping::atLocation(const FunctionContext& fctx,
                                      const Fragment& frag,
                                      llvm::BasicBlock* bb)
{
    if (bb == Fragment::EXIT) {
        assert(frag.getEnd() == Fragment::EXIT);
        return atEnd(fctx, frag);
    }

    llvm::Instruction* point = nullptr;

    // find first non-phi instruction
    for (auto& instr : *bb) {
        if (!llvm::isa<llvm::PHINode>(instr)) {
            point = &instr;
            break;
        }
    }

    assert(point != nullptr);
    return ValueMapping::before(fctx, frag, point);
}

ValueMapping ValueMapping::atBeginning(const FunctionContext& fctx,
                                       const Fragment& frag)
{
    ValueMapping result(fctx, frag, nullptr);
    result.AtBeginning_ = true;
    return result;
}

ValueMapping ValueMapping::atEnd(const FunctionContext& fctx,
                                 const Fragment& frag)
{
    ValueMapping result(fctx, frag, nullptr);
    result.AtEnd_ = true;
    return result;
}

ValueMapping ValueMapping::before(const FunctionContext& fctx,
                                  const Fragment& frag, llvm::Instruction* inst)
{
    // is inst the starting instruction of this fragment?
    if (inst->getParent() == frag.getStart()) {
        auto itr = frag.getStart()->begin();
        while (llvm::isa<llvm::PHINode>(*itr))
            ++itr;

        if (inst == &*itr)
            return ValueMapping::atBeginning(fctx, frag);
    }

    return ValueMapping(fctx, frag, inst);
}

ValueMapping ValueMapping::after(const FunctionContext& fctx,
                                 const Fragment& frag, llvm::Instruction* inst)
{
    assert(!inst->isTerminator());

    // Special case: This fragment is a loop and we're asked for a point after
    // the last instruction in it.
    if (inst->getParent() == frag.getEnd() &&
        frag.getStart() == frag.getEnd()) {

        // is `inst' the last phi instruction in its block?
        if (llvm::isa<llvm::PHINode>(inst)) {
            auto itr = inst->getParent()->begin();
            while (&*itr != inst)
                ++itr;

            ++itr;
            if (itr != inst->getParent()->end() &&
                !llvm::isa<llvm::PHINode>(*itr)) {

                return ValueMapping::atEnd(fctx, frag);
            }
        }
    }

    auto itr = inst->getParent()->begin();
    while (&*itr != inst)
        ++itr;

    // use the next instruction (there must be one)
    ++itr;
    return ValueMapping(fctx, frag, &*itr);
}
} // namespace symbolic_abstraction
