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
 * Collect optimization objectives from interval-style leaves.
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