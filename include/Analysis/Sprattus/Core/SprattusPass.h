/**
 * @file SprattusPass.h
 * @brief LLVM function pass that drives the Sprattus analyzer and rewrites IR.
 */
#pragma once

#include "Analysis/Sprattus/Utils/Utils.h"
#include "Analysis/Sprattus/Domains/SimpleConstProp.h"
#include "Analysis/Sprattus/Utils/Config.h"

#include <llvm/Pass.h>
#include <llvm/IR/CFG.h>

namespace sprattus
{
/**
 * Runs the Sprattus abstract interpreter on a single function and applies
 * local source-level optimizations such as constant propagation and redundant
 * computation elimination to the LLVM IR.
 */
class SprattusPass : public llvm::FunctionPass
{
  public:
    struct Config {
        bool ConstantPropagation = true;
        bool RedundantComputationRemoval = false;
        bool Verbose = false;
    };

  private:
    configparser::Config GlobalConfig_;
    Config Config_;

    /**
     * Returns an AbstratDomain that contains at least every domain given via
     * the commandline and
     *      - appropriate SimpleConstProp AbstractValues if Constant Replacement
     *          should be performed and
     *      - appropriate EqPredicates AbstractValues if Redundant Computation
     *          Eliminatation should be performed.
     */
    DomainConstructor getAugmentedDomain(sprattus::FunctionContext& smtsem);

  public:
    static char ID;

    SprattusPass();
    ~SprattusPass() {}

    /**
     * Replace all uses of Value `from` in `bb` and in the phi-instructions of
     * each successor of `bb` with `to`
     */
    bool replaceUsesOfWithInBBAndPHISuccs(llvm::BasicBlock& bb,
                                          llvm::Value* from, llvm::Value* to);

    /**
     * Performs all replacements of Value uses with constants in `bb` that are
     * allowed by `scp`.
     */
    bool performConstPropForBB(const FunctionContext& fctx,
                               llvm::BasicBlock& bb,
                               const sprattus::domains::SimpleConstProp* scp);

    /**
     * Representation of a partition of a set of llvm Values. Values in the
     * same set behave identical at the considered program point.
     */
    typedef std::vector<std::set<llvm::Value*>> equals_t;

    /**
     *  Insert a new equality (namely of a and b) into the partition eqs.
     */
    void insertEquality(equals_t& eqs, llvm::Value* a, llvm::Value* b);

    /**
     * Find an appropriate replacement candidate for val from the partition eqs
     *
     * That is: find the set from eqs that contains val (if any) and return the
     * most dominating value in this set. As it domainates the definition of
     * val it is also available where val is used.
     */
    llvm::Value* getReplacementCanditdate(const equals_t& eqs,
                                          llvm::Value* val);
    bool performRedundancyReplForBB(const equals_t& eqs, llvm::BasicBlock& bb);

    bool runOnFunction(llvm::Function& function) override;
};
} // namespace sprattus
