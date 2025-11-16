/**
 * @file Analyzer.cpp
 * @brief Fixpoint computation for Sprattus analyzers, including unilateral and
 *        bilateral variants and optional use of incremental SMT solving.
 */
#include "Analysis/Sprattus/Analyzer.h"

#include <llvm/IR/CFG.h>
#include <llvm/Support/Timer.h>

#include "Analysis/Sprattus/utils.h"
#include "Analysis/Sprattus/ValueMapping.h"
#include "Analysis/Sprattus/DomainConstructor.h"
#include "Analysis/Sprattus/repr.h"
#include "Analysis/Sprattus/Config.h"
#include "Analysis/Sprattus/ModuleContext.h"

namespace sprattus
{
Analyzer::Analyzer(const FunctionContext& fctx, const FragmentDecomposition& fd,
                   const DomainConstructor& dom, mode_t mode)
    : FunctionContext_(fctx), Fragments_(fd), Domain_(dom), Mode_(mode),
      CurrentFragment_(nullptr)
{
    // initialize AbstractionPoints_ and FragMap_
    AbstractionPoints_.insert(Fragment::EXIT);
    for (auto& frag : Fragments_) {
        AbstractionPoints_.insert(frag.getStart());
        AbstractionPoints_.insert(frag.getEnd());

        for (auto* loc : frag.locations())
            FragMap_[loc].insert(&frag);
    }

    // initialize Results_ and Stable_
    auto* entry = &fctx.getFunction()->getEntryBlock();
    Stable_.insert(entry);
    Results_[entry] = createInitialValue(Domain_, entry, false);

    // abstract value associated with the entry node has to be top, except if
    // we only deliver dynamic results
    if (mode == FULL && Results_[entry])
        Results_[entry]->havoc();

    // Emit a CSV header in verbose output. Must match rows printed in
    // checkWithStats(). First column always contains "STATS" to make it
    // easy to filter out statistics using grep.
    vout << "STATS,function,fragment,result,time,conflicts,added_eqs\n";
}

std::unique_ptr<Analyzer> Analyzer::New(const FunctionContext& fctx,
                                        const FragmentDecomposition& frag,
                                        const DomainConstructor& domain,
                                        mode_t mode)
{
    std::string variant = fctx.getConfig().get<std::string>(
        "Analyzer", "Variant", "UnilateralAnalyzer");

    if (variant == "UnilateralAnalyzer") {
        return make_unique<UnilateralAnalyzer>(fctx, frag, domain, mode);
    } else if (variant == "BilateralAnalyzer") {
        return make_unique<BilateralAnalyzer>(fctx, frag, domain, mode);
    } else {
        llvm_unreachable("unknown analyzer variant");
    }
}

std::unique_ptr<Analyzer> Analyzer::New(const FunctionContext& fctx,
                                        const FragmentDecomposition& frag,
                                        mode_t mode)
{
    return Analyzer::New(fctx, frag, DomainConstructor(fctx.getConfig()), mode);
}

bool Analyzer::bestTransformer(const AbstractValue* input,
                               const Fragment& fragment,
                               AbstractValue* result) const
{
    assert(Mode_ != ONLY_DYNAMIC);

    VOutBlock vout_block("best transformer for " + repr(fragment));
    CurrentFragment_ = &fragment;
    {
        VOutBlock vb("input");
        vout << *input;
    }

    z3::expr formula = FunctionContext_.formulaFor(fragment);
    ValueMapping vm_before =
        ValueMapping::atBeginning(FunctionContext_, fragment);

    z3::expr av_formula = input->toFormula(vm_before, FunctionContext_.getZ3());

#ifndef NDEBUG
    vout << "Analyzer::bestTransformer input->toFormula {{{\n"
         << av_formula << "\n"
         << "}}}\n";

    if (is_unsat(formula && av_formula)) {
        vout << "Analyzer::bestTransformer input->toFormula is UNSATISFIABLE\n";
    }
#endif

    ValueMapping vm_after = ValueMapping::atEnd(FunctionContext_, fragment);
    bool res = strongestConsequence(result, formula && av_formula, vm_after);

    {
        VOutBlock vb("result");
        vout << *result;
    }

    CurrentFragment_ = nullptr;
    return res;
}

/**
 * Lazily computes the abstract state at the beginning of a basic block.
 *
 * For non-abstraction points, this derives the state by composing a
 * sub-fragment from the closest abstraction point. For abstraction points,
 * it iterates a global fixpoint over all incoming fragments, using the
 * influence relation `Infl_` to invalidate and recompute dependents when
 * a point’s state gets refined. Dynamic results from `ResultStore` are
 * merged in when available.
 */
AbstractValue* Analyzer::at(llvm::BasicBlock* location)
{
    auto store = FunctionContext_.getModuleContext().getResultStore();
    if (store) {
        ResultStore::Key key(0); // Dummy key since dynamic analysis is disabled
        unique_ptr<AbstractValue> res = store->get(key, const_cast<FunctionContext*>(&FunctionContext_));
        if (res) {
            if (!Results_[location]) {
                Results_[location] =
                    createInitialValue(Domain_, location, false);
            }
            Results_[location]->joinWith(*res.get());
            Stable_.insert(location);
            goto end;
        }
    }

    {
        if (AbstractionPoints_.find(location) == AbstractionPoints_.end()) {
            // for non-abstraction-points, only fixpoints are stored in Results_
            if (Results_[location])
                goto end;
            else
                Results_[location] =
                    createInitialValue(Domain_, location, false);

            if (Mode_ == ONLY_DYNAMIC) {
            vout << "Results for non-abstraction point " << repr(location)
                 << " are not being computed in the ONLY_DYNAMIC mode\n";
                // result always will be a bottom
                goto end;
            }

            for (auto* parent_frag : FragMap_[location]) {
                // create a sub-fragment and derive a result from the fragment
                // start
                auto sub_frag = FragmentDecomposition::SubFragment(
                    *parent_frag, parent_frag->getStart(), location);

                VOutBlock vb("Computing result for non-abstraction point: " +
                             repr(sub_frag));

                AbstractValue* input = at(sub_frag.getStart());
                bestTransformer(input, sub_frag, Results_[location].get());
            }

            goto end;
        }

        if (Stable_.find(location) != Stable_.end())
            goto end;

        if (!Results_[location])
            Results_[location] = createInitialValue(Domain_, location, false);

        // in the ONLY_DYNAMIC and ABS_POINTS_DYNAMIC mode we return the result
        // that is already present without computing the fixpoint, i.e., either
        // the dynamically-computed value or bottom
        if (Mode_ != FULL) {
            vout << "Result at abstraction point " << repr(location)
                 << " will not be computed in unsound mode.\n";
            {
                VOutBlock vo("Already-present result");
                vout << repr(*Results_[location].get()) << "\n";
            }
            goto end;
        }

        VOutBlock vo("Computing result at abstraction point: " +
                     repr(location));
        Stable_.insert(location);
        bool updated = false;

        for (auto* frag : FragMap_[location]) {
            if (frag->getEnd() == location) {
                AbstractValue* input = at(frag->getStart());
                AbstractValue* output = Results_[location].get();
                bool this_updated = bestTransformer(input, *frag, output);
                updated = updated || this_updated;
                Infl_[frag->getStart()].insert(location);
            }
        }

        if (updated) {
            std::set<llvm::BasicBlock*> invalid = Infl_[location];
            Infl_[location].clear();

            for (auto* to_update : invalid) {
        vout << "Invalidating " + repr(to_update) + " because " +
                            repr(location) + " was updated.\n";
                Stable_.erase(to_update);
            }

            for (auto* post : invalid) {
                at(post); // force stabilization
            }
        }

        assert(Stable_.find(location) != Stable_.end());
    }

end:

    if (store && Mode_ != ONLY_DYNAMIC) {
        ResultStore::Key key(0); // Dummy key since dynamic analysis is disabled  
        store->put(key, *Results_[location].get());
    }
    return Results_[location].get();
}

/**
 * Returns the abstract state after executing a basic block.
 *
 * If the block is an abstraction point, this applies a single “body-only”
 * transformer to the already stabilized entry state. Otherwise it composes
 * an appropriate sub-fragment ending after the block and applies the best
 * transformer starting from the nearest abstraction point.
 */
AbstractValue* Analyzer::after(llvm::BasicBlock* location)
{
    auto itr = BBEndResults_.find(location);
    if (itr != BBEndResults_.end())
        return itr->second.get(); // return cached result

    BBEndResults_[location] = createInitialValue(Domain_, location, true);
    AbstractValue* output = BBEndResults_[location].get();

    if (Mode_ == ONLY_DYNAMIC)
        return output;

    if (AbstractionPoints_.find(location) != AbstractionPoints_.end()) {
        // compute a single transformer just for the body of this BB
        auto frag =
            FragmentDecomposition::FragmentForBody(FunctionContext_, location);
        VOutBlock vb("Computing result for the body of " + repr(location));
        AbstractValue* input = at(location);
        bestTransformer(input, frag, output);
    } else {
        for (auto* parent_frag : FragMap_[location]) {
            auto sub_frag = FragmentDecomposition::SubFragment(
                *parent_frag, parent_frag->getStart(), location, true);

            VOutBlock vb("Computing result for BB end: " + repr(sub_frag));
            AbstractValue* input = at(sub_frag.getStart());
            bestTransformer(input, sub_frag, output);
        }
    }

    return output;
}

z3::check_result
Analyzer::checkWithStats(z3::solver* solver,
                         std::vector<z3::expr>* assumptions) const
{
    z3::check_result z3_answer;

    // wrap the solver inside time measurements
    llvm::TimeRecord time_rec;
    time_rec -= llvm::TimeRecord::getCurrentTime(true);
    if (assumptions == nullptr || assumptions->size() == 0)
        z3_answer = solver->check();
    else
        z3_answer = solver->check(assumptions->size(), &(*assumptions)[0]);
    time_rec += llvm::TimeRecord::getCurrentTime(false);

    // extract some of the Z3 statistics
    auto stats = solver->statistics();
    unsigned conflicts = 0, added_eqs = 0;
    for (unsigned i = 0; i < stats.size(); ++i) {
        if (stats.key(i) == "conflicts")
            conflicts = stats.uint_value(i);

        if (stats.key(i) == "added eqs")
            added_eqs = stats.uint_value(i);
    }

    // emit a CSV record
    vout << "STATS," << repr(FunctionContext_.getFunction()) << ","
         << (uintptr_t)CurrentFragment_ << "," << repr(z3_answer) << ","
         << time_rec.getWallTime() << "," << conflicts << "," << added_eqs
         << "\n";

    return z3_answer;
}

std::unique_ptr<AbstractValue>
Analyzer::createInitialValue(DomainConstructor& domain, llvm::BasicBlock* bb,
                             bool after) const
{
    if (after)
        return domain.makeBottom(FunctionContext_, bb, after);

    auto store = FunctionContext_.getModuleContext().getResultStore();

    if (!store)
        return domain.makeBottom(FunctionContext_, bb, after);

    ResultStore::Key key(0); // Dummy key since dynamic analysis is disabled
    unique_ptr<AbstractValue> res = store->get(key, const_cast<FunctionContext*>(&FunctionContext_));
    if (!res)
        res = domain.makeBottom(FunctionContext_, bb, after);

    return res;
}

/**
 * Computes the best transformer for a fragment using a unilateral (forward)
 * abstract interpretation scheme.
 *
 * The method optionally reuses an incremental SMT solver per fragment:
 * it caches the fragment’s semantic formula and then, for each distinct
 * input abstract value, adds a guarded copy of its formula under a fresh
 * indicator variable. This allows multiple calls with different inputs
 * to share solver state while keeping them logically separated via
 * assumptions.
 */
bool UnilateralAnalyzer::bestTransformer(const AbstractValue* input,
                                         const Fragment& fragment,
                                         AbstractValue* result) const
{
    VOutBlock vout_block("best transformer for " + repr(fragment));
    CurrentFragment_ = &fragment;
    z3::context& ctx = FunctionContext_.getZ3();
    bool incremental =
        FunctionContext_.getConfig().get<bool>("Analyzer", "Incremental", true);
    std::vector<z3::expr> assumptions;

    {
        VOutBlock vb("input");
        vout << *input;
    }

    // find an appropriate cache entry or create a new one
    unique_ptr<TransfCacheData> temp_entry; // lives through whole function
    TransfCacheData* cache_entry;
    if (incremental && TransfCache_.find(&fragment) != TransfCache_.end()) {
        cache_entry = TransfCache_[&fragment].get();
    } else {
        // Determine whether it's a temporary fragment (in which case we must
        // not store a cache entry for it). If incremental SMT is disabled, we
        // treat everything like a temporary fragment.
        bool temp_frag = true;
        if (incremental) {
            for (auto& frag : Fragments_) {
                if (&frag == &fragment)
                    temp_frag = false;
            }
        }

        // allocate either persistent cache entry or temporary entry
        if (temp_frag) {
            temp_entry.reset(new TransfCacheData(ctx));
            cache_entry = temp_entry.get();
        } else {
            auto& up_ref = TransfCache_[&fragment];
            up_ref.reset(new TransfCacheData(ctx));
            cache_entry = up_ref.get();
        }

        // initialize the entry
        cache_entry->solver.add(FunctionContext_.formulaFor(fragment));
    }

    // generate the formula for the input abstract value
    ValueMapping vm_before =
        ValueMapping::atBeginning(FunctionContext_, fragment);
    z3::expr av_formula = input->toFormula(vm_before, ctx);

    if (incremental) {
        // fill the assumption vector with old indicator input variables
        for (auto& ind_var : cache_entry->ind_vars)
            assumptions.push_back(!ind_var);

        // create a fresh indication variable and construct the input formula
        std::ostringstream ind_var_name;
        ind_var_name << INPUT_VAR_PREFIX << cache_entry->ind_vars.size();
        z3::expr ind_var = ctx.bool_const(ind_var_name.str().c_str());
        cache_entry->solver.add(ind_var == av_formula);
        assumptions.push_back(ind_var);
        cache_entry->ind_vars.push_back(ind_var);
    } else {
        // non-incremental case: don't bother with indicator variables
        cache_entry->solver.add(av_formula);
    }

    ValueMapping vm_after = ValueMapping::atEnd(FunctionContext_, fragment);
    bool res = strongestConsequence(result, vm_after, &cache_entry->solver,
                                    &assumptions);

    {
        VOutBlock vb("result");
        vout << *result;
    }

    CurrentFragment_ = nullptr;
    return res;
}

/**
 * Model-enumeration loop for computing the strongest abstract consequence.
 *
 * Starting from the current abstract value `result`, repeatedly ask the
 * solver for a model that violates `result` (by asserting ¬γ(result)).
 * Each model is turned into a `ConcreteState` and joined into `result`
 * via `updateWith`. Widening is triggered after a configurable number
 * of iterations. The loop terminates once no counterexample model exists,
 * at which point `result` is a greatest fixpoint below the concrete
 * semantics and the original input.
 */
bool UnilateralAnalyzer::strongestConsequence(
    AbstractValue* result, const ValueMapping& vmap, z3::solver* solver,
    std::vector<z3::expr>* assumptions) const
{
    bool changed = false;
    unsigned int loop_count = 0;
    auto config = FunctionContext_.getConfig();
    int widen_delay = config.get<int>("Analyzer", "WideningDelay", 20);
    int widen_frequency =
        config.get<int>("Analyzer", "WideningFrequency", 10);

    while (true) {
        vout << "loop iteration: " << ++loop_count << "\n";
        {
            VOutBlock vob("candidate result");
            vout << *result;
        }

        z3::expr constraint = !result->toFormula(vmap, solver->ctx());
        solver->add(constraint);

        {
            VOutBlock vob("candidate result constraint");
            vout << constraint;
        }

        auto z3_answer = checkWithStats(solver, assumptions);
        assert(z3_answer != z3::unknown);

        if (z3_answer == z3::unsat)
            break;

        vout << "model {{{\n" << solver->get_model() << "}}}\n";

        auto cstate = ConcreteState(vmap, solver->get_model());
        bool here_changed = result->updateWith(cstate);

        if (!here_changed) {
            vout << "ERROR: updateWith() returned false\n";
            {
                VOutBlock vob("faulty abstract value");
                vout << *result;
            }
            assert(here_changed);
        }

        int widen_n = loop_count - widen_delay;
        if (widen_n >= 0 && (widen_n % widen_frequency) == 0) {
            vout << "widening!\n";
            result->widen();
        }

        changed = true;
    }

    return changed;
}

/**
 * Bi-directional version of strongest consequence using widening and narrowing.
 *
 * Maintains a lower bound and an upper bound on the abstract post-state.
 * In each iteration it computes an abstract consequence `p` between them
 * and then either refines the upper bound (when `p` is unsatisfiable with
 * the concrete semantics) or strengthens the lower bound using a concrete
 * counterexample model. The process stops once the upper bound is below
 * the lower bound in the lattice ordering.
 */
bool BilateralAnalyzer::strongestConsequence(AbstractValue* result,
                                             z3::expr phi,
                                             const ValueMapping& vmap) const
{
    using std::unique_ptr;
    bool changed = false;
    z3::solver solver(phi.ctx());
    solver.add(phi);
    unsigned int loop_count = 0;

    auto lower = std::unique_ptr<AbstractValue>(result->clone());

    result->havoc();

    // TODO resource management and timeouts
    while (!((*result) <= (*lower))) {
        vout << "*** lower ***\n" << *lower << "\n";
        vout << "*** upper ***\n" << *result << "\n";
        vout << "loop iteration: " << ++loop_count << "\n";

        auto p = std::unique_ptr<AbstractValue>(lower->clone());
        p->abstractConsequence(*result);

        solver.push();
        solver.add(!p->toFormula(vmap, phi.ctx()));

        auto z3_answer = checkWithStats(&solver);
        assert(z3_answer != z3::unknown);

        if (z3_answer == z3::unsat) {
            vout << "unsat\n"
                 << "p {{{\n"
                 << *p << "}}}\n";
            result->meetWith(*p);
        } else {
            vout << "sat\n"
                 << "model {{{\n"
                 << solver.get_model() << "}}}\n";

            auto cstate = ConcreteState(vmap, solver.get_model());
            if (lower->updateWith(cstate))
                changed = true;
        }
        solver.pop();
    }

    // FIXME changed should be checked differently when we work with
    // overapproximations
    return changed;
}

} // namespace sprattus
