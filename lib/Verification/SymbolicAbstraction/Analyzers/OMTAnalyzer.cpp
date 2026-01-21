/*

 * Author: rainoftime
*/
#include "Verification/SymbolicAbstraction/Analyzers/Analyzer.h"
#include "Verification/SymbolicAbstraction/Core/ConcreteState.h"
#include "Verification/SymbolicAbstraction/Core/ValueMapping.h"
#include "Verification/SymbolicAbstraction/Domains/Intervals.h"
#include "Verification/SymbolicAbstraction/Utils/Config.h"
#include "Verification/SymbolicAbstraction/Utils/Utils.h"

#include <memory>
#include <set>
#include <vector>

namespace symbolic_abstraction {
namespace {
/**
 * @brief Collect optimization objectives from interval-style leaves in an abstract value.
 *
 * Traverses the flattened subcomponents of the abstract value and extracts
 * interval domain components. For each interval that has an associated variable,
 * adds the corresponding Z3 expression to the objectives vector (avoiding duplicates).
 *
 * @param value The abstract value to traverse for interval components
 * @param vmap Value mapping to convert LLVM values to Z3 expressions
 * @param[out] objectives Vector to collect Z3 expressions for optimization objectives
 * @param[in,out] seen Set to track already-seen variables to avoid duplicates
 */
void collectIntervalObjectives(const AbstractValue *value,
                               const ValueMapping &vmap,
                               std::vector<z3::expr> *objectives,
                               std::set<llvm::Value *> *seen) {
  std::vector<const AbstractValue *> leaves;
  value->gatherFlattenedSubcomponents(&leaves);

  for (auto *leaf : leaves) {
    if (auto *iv = dynamic_cast<const domains::Interval *>(leaf)) {
      llvm::Value *var = iv->getVariable();
      if (!var)
        continue;
      if (seen->insert(var).second) {
        objectives->push_back(vmap[var]);
      }
    }
  }
}
} // namespace

/**
 * @brief Run an optimization query to find the maximum or minimum value of an objective.
 *
 * Uses Z3's optimize solver with "box" priority to find an optimal solution for
 * the given objective expression subject to the constraint formula phi. Updates
 * the target abstract value with the concrete state from the optimal model if
 * satisfiable.
 *
 * @param objective The Z3 expression to optimize (typically a variable from vmap)
 * @param phi The constraint formula that must be satisfied
 * @param vmap Value mapping for converting models to concrete states
 * @param[out] target Abstract value to update with the optimal concrete state
 * @param maximize If true, maximize the objective; otherwise minimize
 * @param timeout_ms Timeout in milliseconds (0 means no timeout)
 * @return OptimizeStatus indicating whether the optimization succeeded, failed, or timed out
 */
OMTAnalyzer::OptimizeStatus
OMTAnalyzer::runOptimize(const z3::expr &objective, const z3::expr &phi,
                         const ValueMapping &vmap, AbstractValue *target,
                         bool maximize, unsigned timeout_ms) const {
  z3::optimize opt(phi.ctx());
  opt.add(phi);

  z3::params params(phi.ctx());
  params.set("priority", "box");
  if (timeout_ms > 0)
    params.set("timeout", timeout_ms);
  opt.set(params);

  if (maximize)
    opt.maximize(objective);
  else
    opt.minimize(objective);

  auto res = opt.check();
  if (res == z3::sat) {
    ConcreteState cstate(vmap, opt.get_model());
    target->updateWith(cstate);
    return OptimizeStatus::Sat;
  }

  if (res == z3::unsat)
    return OptimizeStatus::Unsat;

  return OptimizeStatus::Unknown;
}

/**
 * @brief Fallback enumeration-based strongest consequence computation.
 *
 * When OMT optimization fails or times out, this method falls back to a
 * model enumeration approach similar to UnilateralAnalyzer. Iteratively
 * finds counterexample models that violate the current abstract value,
 * joins them in, and applies widening periodically to ensure termination.
 *
 * @param[in,out] result The abstract value to refine (updated in place)
 * @param vmap Value mapping for converting models to concrete states
 * @param phi The constraint formula representing the concrete semantics
 * @return true if the abstract value was changed, false otherwise
 */
bool OMTAnalyzer::fallbackEnumerate(AbstractValue *result,
                                    const ValueMapping &vmap,
                                    const z3::expr &phi) const {
  z3::solver solver(phi.ctx());
  solver.add(phi);

  bool changed = false;
  unsigned int loop_count = 0;
  auto config = FunctionContext_.getConfig();
  int widen_delay = config.get<int>("Analyzer", "WideningDelay", 20);
  int widen_frequency = config.get<int>("Analyzer", "WideningFrequency", 10);

  while (true) {
    z3::expr constraint = !result->toFormula(vmap, solver.ctx());
    solver.add(constraint);

    auto z3_answer = checkWithStats(&solver);
    if (z3_answer == z3::unknown)
      break;
    if (z3_answer == z3::unsat)
      break;

    auto cstate = ConcreteState(vmap, solver.get_model());
    bool here_changed = result->updateWith(cstate);

    int widen_n = (++loop_count) - widen_delay;
    if (widen_n >= 0 && (widen_n % widen_frequency) == 0) {
      result->widen();
    }

    changed = changed || here_changed;
  }

  return changed;
}

/**
 * @brief Compute the strongest abstract consequence using OMT optimization.
 *
 * This is the main entry point for computing strongest consequences in OMTAnalyzer.
 * The algorithm:
 * 1. Checks feasibility of phi
 * 2. If infeasible, sets result to bottom
 * 3. Collects interval objectives from the abstract value
 * 4. For each objective, optimizes both max and min to find tight bounds
 * 5. Joins all optimal solutions into the result
 * 6. Falls back to enumeration if optimization fails or times out
 *
 * @param[in,out] result The abstract value to refine (updated in place)
 * @param phi The constraint formula representing the concrete semantics
 * @param vmap Value mapping for converting models to concrete states
 * @return true if the abstract value was changed, false otherwise
 */
bool OMTAnalyzer::strongestConsequence(AbstractValue *result, z3::expr phi,
                                       const ValueMapping &vmap) const {
  z3::context &ctx = phi.ctx();
  auto cfg = FunctionContext_.getConfig();
  unsigned timeout_ms = cfg.get<int>("Analyzer", "OMTTimeoutMs", 10000);
  bool fallback_on_unknown =
      cfg.get<bool>("Analyzer", "OMTFallbackOnUnknown", true);

  z3::solver feasibility(ctx);
  feasibility.add(phi);
  auto feas_res = checkWithStats(&feasibility);

  if (feas_res == z3::unsat) {
    bool was_bottom = result->isBottom();
    result->resetToBottom();
    return !was_bottom;
  }

  if (feas_res == z3::unknown && fallback_on_unknown)
    return fallbackEnumerate(result, vmap, phi);

  std::vector<z3::expr> objectives;
  std::set<llvm::Value *> seen;
  collectIntervalObjectives(result, vmap, &objectives, &seen);

  if (objectives.empty())
    return fallbackEnumerate(result, vmap, phi);

  auto candidate = std::unique_ptr<AbstractValue>(result->clone());
  candidate->resetToBottom();

  bool saw_unknown = false;
  for (auto &obj : objectives) {
    auto max_res =
        runOptimize(obj, phi, vmap, candidate.get(), true, timeout_ms);
    auto min_res =
        runOptimize(obj, phi, vmap, candidate.get(), false, timeout_ms);

    if (max_res == OptimizeStatus::Unsat || min_res == OptimizeStatus::Unsat) {
      bool was_bottom = result->isBottom();
      result->resetToBottom();
      return !was_bottom;
    }

    if (max_res == OptimizeStatus::Unknown ||
        min_res == OptimizeStatus::Unknown) {
      saw_unknown = true;
    }
  }

  if (saw_unknown && fallback_on_unknown)
    return fallbackEnumerate(result, vmap, phi);

  return result->joinWith(*candidate);
}

} // namespace symbolic_abstraction