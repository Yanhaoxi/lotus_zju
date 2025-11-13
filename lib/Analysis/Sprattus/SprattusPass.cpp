#include "Analysis/Sprattus/SprattusPass.h"

#include "Analysis/Sprattus/utils.h"
#include "Analysis/Sprattus/repr.h"

#include "Analysis/Sprattus/ModuleContext.h"
#include "Analysis/Sprattus/FunctionContext.h"
#include "Analysis/Sprattus/Analyzer.h"
#include "Analysis/Sprattus/MemoryModel.h"
#include "Analysis/Sprattus/DomainConstructor.h"

#include "Analysis/Sprattus/domains/Product.h"
#include "Analysis/Sprattus/ParamStrategy.h"
#include "Analysis/Sprattus/domains/Boolean.h"
#include "Analysis/Sprattus/domains/SimpleConstProp.h"

#include "Analysis/Sprattus/Config.h"

#include <iostream>
#include <deque>

#include <llvm/Pass.h>
#include <llvm/ADT/Statistic.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/ValueSymbolTable.h>
#include <llvm/IR/Dominators.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

using namespace llvm;

#define DEBUG_TYPE "sprattus-pass"
STATISTIC(NumReplacedUses, "Number of replaced uses");

namespace sprattus
{
namespace domains
{
class EqDomain : public BooleanValue
{
  private:
    const RepresentedValue Left_;
    const RepresentedValue Right_;

  protected:
    virtual z3::expr makePredicate(const ValueMapping& vmap) const override
    {
        return vmap[Left_] == vmap[Right_];
    }

  public:
    EqDomain(const FunctionContext& fctx, const RepresentedValue left,
             const RepresentedValue right)
        : BooleanValue(fctx), Left_(left), Right_(right)
    {
    }

    llvm::Value* getLeftVal() const { return Left_; }
    llvm::Value* getRightVal() const { return Right_; }

    virtual void prettyPrint(PrettyPrinter& out) const override
    {
        std::string left_name = Left_->getName().str();
        std::string right_name = Right_->getName().str();

        switch (Val_) {
        case BOTTOM:
            out << pp::bottom;
            break;
        case TOP:
            out << pp::top;
            break;
        case TRUE:
            out << left_name << " == " << right_name;
            break;
        case FALSE:
            out << left_name << " != " << right_name;
            break;
        }
    }

    virtual ~EqDomain() {}

    virtual AbstractValue* clone() const override
    {
        return new EqDomain(*this);
    }

    virtual bool isJoinableWith(const AbstractValue& other) const override
    {
        auto* other_val = static_cast<const EqDomain*>(&other);
        if (other_val) {
            if (other_val->Left_ == Left_ && other_val->Right_ == Right_) {
                return true;
            }
        }
        return false;
    }
};
}
namespace // unnamed
{
using namespace llvm;

/**
  *  Check whether the given AbstractValue contains an AbstractValue belonging
  *  to the template parameter.
  *
  *  Example call:
  *      containsDomain<SimpleConstProp>(val_ptr)
  */
template <typename T> bool containsDomain(const sprattus::AbstractValue* value)
{
    bool res = false;

    // get flat vector of resulting AbstractValues
    std::vector<const AbstractValue*> vec;
    value->gatherFlattenedSubcomponents(&vec);
    for (auto inner_val : vec) {
        if (static_cast<const T*>(inner_val)) {
            res = true;
            break;
        }
    }
    return res;
}
} // namespace unnamed

char SprattusPass::ID;

SprattusPass::SprattusPass() : llvm::FunctionPass(SprattusPass::ID)
{
    const char* M = "SprattusPass";

    Config_.ConstantPropagation =
        GlobalConfig_.get<bool>(M, "ConstantPropagation", true);
    Config_.RedundantComputationRemoval =
        GlobalConfig_.get<bool>(M, "RedundantComputationRemoval", false);
    Config_.Verbose = GlobalConfig_.get<bool>(M, "Verbose", false);

    VerboseEnable = Config_.Verbose;
}

DomainConstructor
SprattusPass::getAugmentedDomain(sprattus::FunctionContext& smtsem)
{
    using namespace sprattus;
    using namespace domains;

    DomainConstructor domain(smtsem.getConfig());

    bool contains_cp = containsDomain<SimpleConstProp>(
        domain.makeBottom(smtsem, Fragment::EXIT, false).get());

    bool contains_eqres = containsDomain<EqDomain>(
        domain.makeBottom(smtsem, Fragment::EXIT, false).get());

    bool needs_cp = Config_.ConstantPropagation && !contains_cp;
    bool needs_eqres = Config_.RedundantComputationRemoval && !contains_eqres;

    if (needs_cp && needs_eqres) {
        vout << "Adding SimpleConstProp and EqRes to domain." << std::endl;
        return DomainConstructor(
            domain.name() + "+consts", "",
            [=](const FunctionContext& fctx, BasicBlock* for_bb, bool after) {
                domains::Product* p = new domains::Product(fctx);
                p->add(domain.makeBottom(fctx, for_bb, after));
                p->add(params::ForValues<SimpleConstProp>(fctx, for_bb, after));
                p->add(params::ForValuePairsRestricted<EqDomain>(fctx, for_bb,
                                                                 after));
                p->finalize();
                return unique_ptr<AbstractValue>(p);
            });
    }
    if (needs_cp) {
        vout << "Adding SimpleConstProp to domain." << std::endl;
        return DomainConstructor(
            domain.name() + "+consts", "",
            [=](const FunctionContext& fctx, BasicBlock* for_bb, bool after) {
                domains::Product* p = new domains::Product(fctx);
                p->add(domain.makeBottom(fctx, for_bb, after));
                p->add(params::ForValues<SimpleConstProp>(fctx, for_bb, after));
                p->finalize();
                return unique_ptr<AbstractValue>(p);
            });
    }
    if (needs_eqres) {
        vout << "Adding EqRes to domain." << std::endl;
        return DomainConstructor(
            domain.name() + "+consts", "",
            [=](const FunctionContext& fctx, BasicBlock* for_bb, bool after) {
                domains::Product* p = new domains::Product(fctx);
                p->add(domain.makeBottom(fctx, for_bb, after));
                p->add(params::ForValuePairsRestricted<EqDomain>(fctx, for_bb,
                                                                 after));
                p->finalize();
                return unique_ptr<AbstractValue>(p);
            });
    }

    return domain;
}

bool SprattusPass::replaceUsesOfWithInBBAndPHISuccs(BasicBlock& bb, Value* from,
                                                    Value* to)
{
    bool changed = false;

    // get a reasonable textual representation of a Value
    auto getText = [](Value* val) -> std::string {
        if (auto* v = dyn_cast_or_null<Constant>(val)) {
            if (llvm::isa<ConstantPointerNull>(v))
                return "nullptr";
            else if (llvm::isa<ConstantExpr>(v))
                return "some constant expression";
            else
                return std::to_string(v->getUniqueInteger().getZExtValue());
        } else {
            return "`" + val->getName().str() + "`";
        }
    };

    for (auto& inst : bb) {
        bool found_use = false;
        for (auto arg : inst.operand_values()) {
            if (arg == from) {
                found_use = true;
                ++NumReplacedUses;
            }
        }
        if (found_use) {
            // we found a use. This is relevant for the value of changed
            inst.replaceUsesOfWith(from, to);
            changed = true;
            vout << "  Replaced use of `" << from->getName().str()
                 << "` by value " << getText(to) << " in `"
                 << inst.getName().str() << "` (bb: `" << bb.getName().str()
                 << "`)" << std::endl;
        }
    }

    // Replace constant arguments of PHIs in successor bbs.
    // This might be necessary as they might not be contained
    // in the successors AbstractValue.
    for (auto it = succ_begin(&bb); it != succ_end(&bb); ++it) {
        for (auto& inst : **it) {
            if (auto phi = dyn_cast<PHINode>(&inst)) {
                if (phi->getIncomingValueForBlock(&bb) == from) {
                    int idx = phi->getBasicBlockIndex(&bb);
                    phi->setIncomingValue(idx, to);
                    changed = true;
                    ++NumReplacedUses;
                    vout << "  Replaced PHI use of `" << from->getName().str()
                         << "` by value " << getText(to) << " in `"
                         << inst.getName().str() << "` (bb: `"
                         << bb.getName().str() << "`)" << std::endl;
                }
            } else {
                break;
            }
        }
    }
    return changed;
}

bool SprattusPass::performConstPropForBB(
    const FunctionContext& fctx, BasicBlock& bb,
    const sprattus::domains::SimpleConstProp* scp)
{
    if (!scp->isConst())
        return false;

    llvm::Value* var = scp->getVariable();
    uint64_t val = scp->getConstValue();
    z3::sort sort = fctx.sortForType(var->getType());

    if (!sort.is_bv())
        return false;

    int bw = sort.bv_size();

    // Create llvm constant with identical type to eliminate use
    auto type = var->getType();
    auto apval = APInt(bw, val, false);
    Value* const_int = Constant::getIntegerValue(type, apval);

    return replaceUsesOfWithInBBAndPHISuccs(bb, var, const_int);
}

void SprattusPass::insertEquality(equals_t& eqs, Value* a, Value* b)
{
    // If x is presenet in a set of eqs, inserts y into the same set and
    // returns `true`. Returns `false` otherwise.
    auto insertConditionally = [&eqs](Value* x, Value* y) {
        for (auto& part : eqs) {
            if (part.find(x) != part.end()) {
                part.insert(y);
                return true;
            }
        }
        return false;
    };

    if (!insertConditionally(a, b) && !insertConditionally(b, a)) {
        // Create new class of equal Values
        eqs.emplace_back();
        eqs[eqs.size() - 1].insert(a);
        eqs[eqs.size() - 1].insert(b);
    }
}

Value* SprattusPass::getReplacementCanditdate(const equals_t& eqs, Value* val)
{
    Instruction* candidate = dyn_cast_or_null<Instruction>(val);
    if (!candidate)
        return nullptr; // we only want to replace instructions

    llvm::DominatorTreeWrapperPass pass;
    pass.runOnFunction(*candidate->getParent()->getParent());
    llvm::DominatorTree& dt = pass.getDomTree();

    for (auto& eq : eqs) {
        // find the set that contains val
        if (eq.find(val) != eq.end()) {
            // find most dominating Value in set
            for (auto oth : eq) {
                if (auto* oth_inst = dyn_cast_or_null<Instruction>(oth)) {
                    if (dt.dominates(oth_inst, candidate)) {
                        candidate = oth_inst;
                    }
                } else {
                    return oth;
                    // replacing with something that is not an instruction
                    // is always good as it means no recomputation
                }
            }
            break;
        }
    }
    if (candidate == val)
        return nullptr;
    else
        return candidate;
}

bool SprattusPass::performRedundancyReplForBB(const equals_t& eqs,
                                              BasicBlock& bb)
{
    using namespace sprattus;
    using namespace domains;
    std::map<Value*, Value*> repl;
    bool changed = false;

    // Try to compute for each value another value which we can replace it with
    vout << "  equalities for " << bb.getName().str() << ": [" << std::endl;
    for (auto& eq : eqs) {
        vout << "    [";
        bool first = true;
        for (auto val : eq) {
            if (!first)
                vout << ", ";
            first = false;
            vout << val->getName().str();
            repl[val] = getReplacementCanditdate(eqs, val);
            vout << " -> " << (repl[val] ? repl[val]->getName().str() : "NONE");
        }
        vout << "]" << std::endl;
    }
    vout << "  ]" << std::endl;

    // Perform the replacements with the values that we found
    for (auto& eq : eqs) {
        for (auto val : eq) {
            if (repl[val]) {
                bool tmp = replaceUsesOfWithInBBAndPHISuccs(bb, val, repl[val]);
                changed = changed || tmp;
            }
        }
    }

    return changed;
}

bool SprattusPass::runOnFunction(llvm::Function& function)
{
    vout << "Perform SprattusPass on function `" << function.getName().str()
         << "'." << std::endl
         << "¸.·´¯`·.´¯`·.¸¸.·´¯`·.¸><(((º>" << std::endl
         << std::endl;
    bool changed = false;

    using namespace sprattus;
    using namespace domains;

    // Create a ModuleContext object to create FunctionContexts
    auto mctx =
        make_unique<const ModuleContext>(function.getParent(), GlobalConfig_);

    // Create the FunctionContext object that is used for the analysis
    auto fctx = mctx->createFunctionContext(&function);

    // Generate the FragmentDecomposition that is specified by the Config_ field
    // of the FunctionContext
    auto fragment_decomp = FragmentDecomposition::For(*fctx.get());
    vout << "Fragment decomposition: " << fragment_decomp << endl;

    // add necessary components to domain if not yet contained
    DomainConstructor domain = getAugmentedDomain(*fctx.get());
    auto analyzer = Analyzer::New(*fctx.get(), fragment_decomp, domain);

    std::vector<const AbstractValue*> results;
    equals_t equalities;

    vout << "Analysis Results {{{" << std::endl;
    for (llvm::BasicBlock& bb : function) {
        results.clear();
        equalities.clear();

        // Compute and gather the analysis results for this basic block
        analyzer->after(&bb)->gatherFlattenedSubcomponents(&results);

        // Perform the actual transformations for Constant Replacement
        // and find equal values for Redundant Computation Elimination
        for (auto& val : results) {
            if (auto scp = static_cast<const SimpleConstProp*>(val)) {
                // Constant Replacement transformation
                if (!Config_.ConstantPropagation)
                    continue; // Do not perform this transformation
                bool res = performConstPropForBB(*fctx.get(), bb, scp);
                changed = changed || res;
            } else if (auto pred = static_cast<const EqDomain*>(val)) {
                // Redundant Computation Elimination transformation
                if (!Config_.RedundantComputationRemoval)
                    continue; // Do not perform this transformation
                if (pred->getValue() == BooleanValue::TRUE) {
                    insertEquality(equalities, pred->getLeftVal(),
                                   pred->getRightVal());
                }
            }
        }

        if (Config_.RedundantComputationRemoval) {
            // Redundant Computation Elimination transformation
            // perform the actual transformation
            bool res = performRedundancyReplForBB(equalities, bb);
            changed = changed || res;
        }
    }

    vout << "}}}" << std::endl
         << "DONE." << std::endl;
    return changed;
}
} // namespace sprattus
