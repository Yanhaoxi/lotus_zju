/**
 * @file QuickSampler.cpp
 * @brief Implementation of quick_sampler - a mutation-based approach for sampling SMT formulas
 *
 * This implementation generates diverse models by flipping variable assignments
 * and exploring the solution space through mutations.
 */

#include "Solvers/SMT/SMTSampler/SMTSampler.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace std;
using namespace z3;

namespace {
constexpr const char *kSamplerName = "QuickSampler";

void log_info(const std::string &msg) {
  std::cout << "[" << kSamplerName << "] " << msg << '\n';
}

void log_warn(const std::string &msg) {
  std::cerr << "[" << kSamplerName << "] WARN: " << msg << '\n';
}

void log_error(const std::string &msg) {
  std::cerr << "[" << kSamplerName << "] ERROR: " << msg << '\n';
}
} // namespace

class quick_sampler {
  std::string input_file;

  struct timespec start_time;
  double solver_time = 0.0;
  int max_samples;
  double max_time;

  z3::context c;
  z3::optimize opt;
  std::vector<int> ind;
  std::unordered_set<int> unsat_vars;
  int epochs = 0;
  int flips = 0;
  int samples = 0;
  int solver_calls = 0;
  bool stop_requested = false;
  std::string stop_reason;

  std::mt19937 rng;
  std::uniform_int_distribution<int> bit_dist{0, 1};

  std::ofstream results_file;

public:
  quick_sampler(std::string input, int max_samples, double max_time)
      : input_file(input), max_samples(max_samples), max_time(max_time),
        opt(c) {
    auto seed = static_cast<uint64_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    rng.seed(seed);
  }

  void run() {
    clock_gettime(CLOCK_REALTIME, &start_time);
    if (!parse_cnf()) {
      log_error("Failed to parse CNF input: " + input_file);
      return;
    }
    results_file.open(input_file + ".samples", std::ios::out | std::ios::trunc);
    if (!results_file.is_open()) {
      log_error("Failed to open output file: " + input_file + ".samples");
      return;
    }
    results_file << "# format: <mutations>: <bitstring>\n";
    while (true) {
      opt.push();
      for (int v : ind) {
        if (bit_dist(rng))
          opt.add(literal(v), 1);
        else
          opt.add(!literal(v), 1);
      }
      if (!solve()) {
        opt.pop();
        break;
      }
      z3::model m = opt.get_model();
      opt.pop();

      sample(m);
      print_stats(false);
    }
    if (stop_requested) {
      log_info("Stopped due to " + stop_reason);
    }
    finish();
  }

  void print_stats(bool simple) {
    struct timespec end;
    clock_gettime(CLOCK_REALTIME, &end);
    double elapsed = duration(&start_time, &end);
    std::cout << "Samples " << samples << '\n';
    std::cout << "Execution time " << elapsed << '\n';
    if (simple)
      return;
    std::cout << "Solver time: " << solver_time << '\n';
    std::cout << "Epochs " << epochs << ", Flips " << flips << ", Unsat "
              << unsat_vars.size() << ", Calls " << solver_calls << '\n';
  }

  bool parse_cnf() {
    z3::expr_vector exp(c);
    std::ifstream f(input_file);
    if (!f.is_open()) {
      log_error("Unable to open input file");
      return false;
    }
    std::unordered_set<int> indset;
    bool has_ind = false;
    int max_var = 0;
    std::string line;
    while (getline(f, line)) {
      if (line.empty())
        continue;
      std::istringstream iss(line);
      if (line.find("c ind ") == 0) {
        std::string s;
        iss >> s;
        iss >> s;
        int v;
        while (iss >> v) {
          if (v && indset.find(v) == indset.end()) {
            indset.insert(v);
            ind.push_back(v);
            has_ind = true;
          }
        }
      } else if (line[0] != 'c' && line[0] != 'p') {
        z3::expr_vector clause(c);
        int v;
        while (iss >> v) {
          if (v > 0)
            clause.push_back(literal(v));
          else if (v < 0)
            clause.push_back(!literal(-v));
          v = abs(v);
          if (!has_ind && v != 0)
            indset.insert(v);
          if (v > max_var)
            max_var = v;
        }
        exp.push_back(mk_or(clause));
      }
    }
    f.close();
    if (!has_ind) {
      for (int lit = 1; lit <= max_var; ++lit) {
        if (indset.find(lit) != indset.end()) {
          ind.push_back(lit);
        }
      }
    }
    if (ind.empty()) {
      log_warn("No independent variables found in CNF");
    }
    z3::expr formula = mk_and(exp);
    opt.add(formula);
    return true;
  }

  void sample(z3::model &m) {
    std::unordered_set<std::string> initial_mutations;
    std::string m_string = model_string(m);
    if (m_string.size() != ind.size()) {
      log_error("Model projection size mismatch; skipping sample");
      return;
    }
    std::cout << m_string << " STARTING\n";
    output(m_string, 0);
    opt.push();
    for (unsigned i = 0; i < ind.size(); ++i) {
      int v = ind[i];
      if (m_string[i] == '1')
        opt.add(literal(v), 1);
      else
        opt.add(!literal(v), 1);
    }

    std::unordered_map<std::string, int> mutations;
    for (unsigned i = 0; i < ind.size(); ++i) {
      if (unsat_vars.find(i) != unsat_vars.end())
        continue;
      opt.push();
      int v = ind[i];
      if (m_string[i] == '1')
        opt.add(!literal(v));
      else
        opt.add(literal(v));
      if (solve()) {
        z3::model new_model = opt.get_model();
        std::string new_string = model_string(new_model);
        if (initial_mutations.find(new_string) == initial_mutations.end()) {
          initial_mutations.insert(new_string);
          // std::cout << new_string << '\n';
          std::unordered_map<std::string, int> new_mutations;
          new_mutations[new_string] = 1;
          output(new_string, 1);
          flips += 1;
          for (auto &it : mutations) {
            if (it.second >= 6)
              continue;
            std::string candidate;
            for (unsigned j = 0; j < ind.size(); ++j) {
              bool a = m_string[j] == '1';
              bool b = it.first[j] == '1';
              bool c = new_string[j] == '1';
              if (a ^ ((a ^ b) | (a ^ c)))
                candidate += '1';
              else
                candidate += '0';
            }
            if (mutations.find(candidate) == mutations.end() &&
                new_mutations.find(candidate) == new_mutations.end()) {
              new_mutations[candidate] = it.second + 1;
              output(candidate, it.second + 1);
            }
          }
          for (auto &it : new_mutations) {
            mutations[it.first] = it.second;
          }
        } else {
          // std::cout << new_string << " repeated\n";
        }
      } else {
        log_warn("Mutation unsat at index " + std::to_string(i));
        unsat_vars.insert(i);
      }
      opt.pop();
      print_stats(true);
    }
    epochs += 1;
    opt.pop();
  }

  void output(std::string &sample, int nmut) {
    samples += 1;
    results_file << nmut << ": " << sample << '\n';
  }

  void finish() {
    print_stats(false);
    if (results_file.is_open()) {
      results_file.close();
      log_info("Samples file closed");
    }
  }

  bool solve() {
    struct timespec start;
    clock_gettime(CLOCK_REALTIME, &start);
    double elapsed = duration(&start_time, &start);
    if (elapsed > max_time) {
      stop_requested = true;
      stop_reason = "timeout";
      log_info("Stopping: timeout");
      return false;
    }
    if (samples >= max_samples) {
      stop_requested = true;
      stop_reason = "samples";
      log_info("Stopping: sample limit");
      return false;
    }

    z3::check_result result = opt.check();
    struct timespec end;
    clock_gettime(CLOCK_REALTIME, &end);
    solver_time += duration(&start, &end);
    solver_calls += 1;

    if (result == z3::unknown) {
      log_warn("Solver returned unknown");
    }
    return result == z3::sat;
  }

  std::string model_string(z3::model &model) {
    std::string s;

    for (int v : ind) {
      z3::expr b = model.eval(literal(v), true);
      if (b.is_true()) {
        s += "1";
      } else {
        s += "0";
      }
    }
    return s;
  }

  double duration(struct timespec *a, struct timespec *b) {
    return (b->tv_sec - a->tv_sec) + 1.0e-9 * (b->tv_nsec - a->tv_nsec);
  }

  z3::expr literal(int v) {
    return c.constant(c.str_symbol(std::to_string(v).c_str()), c.bool_sort());
  }
};
