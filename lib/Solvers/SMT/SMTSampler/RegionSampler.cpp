/**
 * @file RegionSampler.cpp
 * @brief Abstraction-based sampling using SymAbs + hit-and-run walk
 *
 * This sampler builds a linear integer abstraction of a bit-vector formula
 * (via SymAbs) and samples integer points from the abstract polytope using a
 * hit-and-run random walk. Candidate points are validated against the original
 * SMT formula by evaluating the model.
 */

#include "Solvers/SMT/SMTSampler/SMTSampler.h"

#include <fstream>
#include <iostream>
#include <random>
#include <unordered_map>

#include "Solvers/SMT/SMTSampler/PolySampler/PolySampler.h"
#include "Solvers/SMT/SymAbs/SymAbsUtils.h"
#include "Solvers/SMT/SymAbs/SymbolicAbstraction.h"

using namespace std;
using namespace z3;

namespace {
constexpr const char *kSamplerName = "RegionSampler";

void log_warn(const std::string &msg) {
  std::cerr << "[" << kSamplerName << "] WARN: " << msg << '\n';
}

void log_error(const std::string &msg) {
  std::cerr << "[" << kSamplerName << "] ERROR: " << msg << '\n';
}

struct VarInfo {
  z3::expr var;
  unsigned width;
  std::string name;
};

static bool signed_range(unsigned width, int64_t &min_out, int64_t &max_out) {
  if (width == 0 || width > 63)
    return false;
  int64_t pow2 = 1LL << static_cast<int>(width - 1);
  min_out = -pow2;
  max_out = pow2 - 1;
  return true;
}

static z3::expr bv_from_int(z3::context &ctx, int64_t value, unsigned width) {
  uint64_t u = static_cast<uint64_t>(value);
  if (width < 64) {
    uint64_t mask = (1ULL << width) - 1;
    u &= mask;
  }
  return ctx.bv_val(u, width);
}

static bool model_satisfies(const z3::expr &phi,
                            const std::vector<VarInfo> &vars,
                            const std::vector<int64_t> &point) {
  z3::model m(phi.ctx());
  for (size_t i = 0; i < vars.size(); ++i) {
    z3::func_decl decl = vars[i].var.decl();
    z3::expr val = bv_from_int(phi.ctx(), point[i], vars[i].width);
    m.add_const_interp(decl, val);
  }
  return m.eval(phi, true).is_true();
}

} // namespace

struct region_sampler {
  std::string input_file;
  int max_samples = 1000;
  double max_time_ms = 30000.0;
  RegionSampling::SampleConfig sample_config;

  SymAbs::AbstractionConfig abs_config;

  enum class Domain { Zone, Octagon };
  Domain domain = Domain::Octagon;

  RegionSampling::Walk walk = RegionSampling::Walk::HitAndRun;

  z3::context c;
  z3::expr smt_formula;
  std::vector<VarInfo> vars;
  std::vector<RegionSampling::LinearConstraint> constraints;

  std::mt19937_64 rng;

  explicit region_sampler(std::string input, int max_samples, double max_time)
      : input_file(std::move(input)), max_samples(max_samples),
        max_time_ms(max_time), smt_formula(c) {
    rng.seed(static_cast<uint64_t>(std::chrono::high_resolution_clock::now()
                                       .time_since_epoch()
                                       .count()));
  }

  void parse_smt() {
    try {
      expr_vector evec = c.parse_file(input_file.c_str());
      smt_formula = mk_and(evec);
    } catch (const z3::exception &e) {
      log_error(std::string("Failed to parse SMT file: ") + e.msg());
      smt_formula = z3::expr(c);
    }
  }

  /**
   * @brief Collects all bit-vector variables from the SMT formula.
   *
   * Iterates through all variables in the formula and selects only those
   * of bit-vector sort. Stores them in the `vars` vector.
   */
  void collect_vars() {
    expr_vector all_vars(c);
    get_expr_vars(smt_formula, all_vars);
    vars.clear();
    for (unsigned i = 0; i < all_vars.size(); ++i) {
      if (!all_vars[i].get_sort().is_bv())
        continue;
      VarInfo info{all_vars[i], all_vars[i].get_sort().bv_size(),
                   all_vars[i].decl().name().str()};
      vars.push_back(info);
    }
  }

  /**
   * @brief Builds linear integer constraints from the SMT formula.
   *
   * Uses Symbolic Abstraction (SymAbs) to abstract the SMT formula into
   * a set of linear constraints (Zone or Octagon domain).
   * Also adds bounds for bit-vector variables.
   *
   * @return true if constraints were successfully built, false otherwise.
   */
  bool build_constraints() {
    std::unordered_map<std::string, size_t> index;
    for (size_t i = 0; i < vars.size(); ++i) {
      index[vars[i].name] = i;
    }

    constraints.clear();
    if (domain == Domain::Zone) {
      auto zone = SymAbs::alpha_zone_V(smt_formula, extract_exprs(),
                                       abs_config);
      for (const auto &cstr : zone) {
        RegionSampling::LinearConstraint c;
        c.coeffs.assign(vars.size(), 0);
        auto it_i = index.find(cstr.var_i.decl().name().str());
        if (it_i == index.end())
          continue;
        c.coeffs[it_i->second] += 1;
        if (!cstr.unary) {
          auto it_j = index.find(cstr.var_j.decl().name().str());
          if (it_j == index.end())
            continue;
          c.coeffs[it_j->second] -= 1;
        }
        c.bound = cstr.bound;
        constraints.push_back(std::move(c));
      }
    } else {
      auto oct = SymAbs::alpha_oct_V(smt_formula, extract_exprs(),
                                     abs_config);
      for (const auto &cstr : oct) {
        RegionSampling::LinearConstraint c;
        c.coeffs.assign(vars.size(), 0);
        auto it_i = index.find(cstr.var_i.decl().name().str());
        if (it_i == index.end())
          continue;
        c.coeffs[it_i->second] += cstr.lambda_i;
        if (!cstr.unary) {
          auto it_j = index.find(cstr.var_j.decl().name().str());
          if (it_j == index.end())
            continue;
          c.coeffs[it_j->second] += cstr.lambda_j;
        }
        c.bound = cstr.bound;
        constraints.push_back(std::move(c));
      }
    }

    // Add bit-width bounds for each variable
    for (size_t i = 0; i < vars.size(); ++i) {
      int64_t min_v = 0;
      int64_t max_v = 0;
      if (!signed_range(vars[i].width, min_v, max_v)) {
        continue;
      }
      RegionSampling::LinearConstraint upper;
      upper.coeffs.assign(vars.size(), 0);
      upper.coeffs[i] = 1;
      upper.bound = max_v;
      constraints.push_back(std::move(upper));

      RegionSampling::LinearConstraint lower;
      lower.coeffs.assign(vars.size(), 0);
      lower.coeffs[i] = -1;
      lower.bound = -min_v;
      constraints.push_back(std::move(lower));
    }

    return !constraints.empty();
  }

  std::vector<z3::expr> extract_exprs() const {
    std::vector<z3::expr> out;
    out.reserve(vars.size());
    for (const auto &v : vars)
      out.push_back(v.var);
    return out;
  }

  /**
   * @brief Finds an initial satisfying assignment using the SMT solver.
   *
   * This point serves as the starting point for the random walk.
   *
   * @param point [out] The found point as a vector of integers.
   * @return true if a point was found, false otherwise (UNSAT or error).
   */
  bool initial_point(std::vector<int64_t> &point) {
    solver s(c);
    s.add(smt_formula);
    if (s.check() != sat)
      return false;
    model m = s.get_model();
    point.clear();
    point.reserve(vars.size());
    for (const auto &v : vars) {
      int64_t val = 0;
      if (!SymAbs::eval_model_value(m, v.var, val))
        return false;
      point.push_back(val);
    }
    return true;
  }

  /**
   * @brief Main execution function for RegionSampler.
   *
   * 1. Parses SMT formula.
   * 2. Collects bit-vector variables.
   * 3. Builds abstraction constraints (polytope).
   * 4. Finds an initial valid point.
   * 5. Runs the random walk sampling (RegionSampling::sample_points).
   * 6. Writes samples to output file.
   */
  void run() {
    parse_smt();
    if (!smt_formula) {
      log_error("No SMT formula loaded; aborting run");
      return;
    }
    collect_vars();
    if (vars.empty()) {
      log_warn("No bit-vector variables found");
      return;
    }
    if (!build_constraints()) {
      log_warn("No abstraction constraints built");
      return;
    }

    std::vector<int64_t> point;
    if (!initial_point(point)) {
      log_warn("Formula unsat or model extraction failed");
      return;
    }

    std::ofstream out(input_file + ".abs.samples");
    if (!out.is_open()) {
      log_error("Failed to open output file: " + input_file + ".abs.samples");
      return;
    }
    // Write header (variable names)
    for (size_t i = 0; i < vars.size(); ++i) {
      if (i)
        out << " ";
      out << vars[i].name;
    }
    out << "\n";

    sample_config.max_samples = max_samples;
    sample_config.max_time_ms = max_time_ms;
    // Acceptance criterion: must satisfy original SMT formula
    auto accept = [this](const std::vector<int64_t> &candidate) {
      return model_satisfies(smt_formula, vars, candidate);
    };

    auto samples = RegionSampling::sample_points(constraints, point, walk, rng,
                                                 sample_config, accept);
    for (const auto &sample : samples) {
      for (size_t i = 0; i < sample.size(); ++i) {
        if (i)
          out << " ";
        out << sample[i];
      }
      out << "\n";
    }
  }
};
