/**
 * @file IntervalSampler.cpp
 * @brief Implementation of interval_sampler - a bounds-based approach for sampling SMT formulas
 *
 * This implementation samples models by determining variable bounds and randomly
 * selecting values within those bounds.
 */

#include "Solvers/SMT/SMTSampler/SMTSampler.h"

#include <chrono>
#include <ctime>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <random>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>

using namespace std;
using namespace z3;

namespace {
constexpr const char *kSamplerName = "IntervalSampler";

void log_info(const std::string &msg) {
  std::cout << "[" << kSamplerName << "] " << msg << '\n';
}

void log_warn(const std::string &msg) {
  std::cerr << "[" << kSamplerName << "] WARN: " << msg << '\n';
}

void log_error(const std::string &msg) {
  std::cerr << "[" << kSamplerName << "] ERROR: " << msg << '\n';
}

std::string join_path(const std::string &dir, const std::string &name) {
  if (dir.empty())
    return name;
  if (dir.back() == '/')
    return dir + name;
  return dir + "/" + name;
}
} // namespace

struct interval_sampler {
  std::string path;
  std::string input_file;
  std::vector<std::string> input_files;
  //  bool filemode;

  struct timespec start_time;
  double solver_time = 0.0;
  double check_time = 0.0;
  int max_samples;
  double max_time;

  int m_samples = 0;
  int m_success = 0;
  int m_unique = 0;
  double m_sample_time = 0.0;
  bool stop_requested = false;
  std::string stop_reason;

  z3::context c;
  z3::expr smt_formula;
  z3::expr_vector m_vars;

  // std::vector<z3::expr> smt_formulas;
  std::vector<int> lower_bounds;
  std::vector<int> upper_bounds;
  std::vector<bool> should_fix;

  std::vector<std::vector<int>> unique_models;

  std::mt19937 rng;

  interval_sampler(std::string &input, int max_samples, double max_time)
      : path(input), max_samples(max_samples), max_time(max_time),
        smt_formula(c), m_vars(c) {
    auto seed = static_cast<uint64_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    rng.seed(seed);
    struct stat info = {};
    if (stat(path.c_str(), &info) == 0) {
      if (info.st_mode & S_IFDIR) {
        DIR *dirp = opendir(input.c_str());
        struct dirent *dp;
        while ((dp = readdir(dirp)) != NULL) {
          std::string tmp(dp->d_name);
          if (tmp == "." || tmp == "..")
            continue;
          input_files.push_back(join_path(path, tmp));
        }
        closedir(dirp);
      } else {
        input_files.push_back(input);
      }
    } else {
      input_files.push_back(input);
    }
    input_file = input;
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

  void get_bounds() {
    params p(c);
    p.set("priority", c.str_symbol("box"));
    p.set("timeout", (unsigned)15000); // 15 seconds
    // 'core_maxsat', 'wmax', 'maxres', 'pd-maxres' ?
    // p.set("maxsat_engine", c.str_symbol("maxres"));

    optimize opt_sol_min(c);
    opt_sol_min.set(p);
    opt_sol_min.add(smt_formula);

    optimize opt_sol_max(c);
    opt_sol_max.set(p);
    opt_sol_max.add(smt_formula);

    // Find min
    std::vector<optimize::handle> handlers_min;
    for (unsigned i = 0; i < m_vars.size(); i++) {
      handlers_min.push_back(opt_sol_min.minimize(m_vars[i]));
    }
    auto min_res = opt_sol_min.check();
    if (min_res != sat) {
      log_warn("Minimize check not sat; using defaults");
    }
    for (unsigned i = 0; i < m_vars.size(); i++) {
      // std::cout << m_vars[i] <<": " << opt_sol_min.upper(handlers_min[i]) <<
      // "\n";
      if (min_res == sat) {
        lower_bounds.push_back(
            opt_sol_min.upper(handlers_min[i]).get_numeral_int());
      } else {
        lower_bounds.push_back(0);
      }
    }

    // Find max
    std::vector<optimize::handle> handlers_max;
    for (unsigned i = 0; i < m_vars.size(); i++) {
      handlers_max.push_back(opt_sol_max.maximize(m_vars[i]));
    }
    auto max_res = opt_sol_max.check();
    if (max_res != sat) {
      log_warn("Maximize check not sat; using defaults");
    }
    for (unsigned i = 0; i < m_vars.size(); i++) {
      // std::cout << m_vars[i] <<": " << opt_sol_max.lower(handlers_max[i]) <<
      // "\n";
      if (max_res == sat) {
        upper_bounds.push_back(
            opt_sol_max.lower(handlers_max[i]).get_numeral_int());
      } else {
        unsigned sz = m_vars[i].get_sort().bv_size();
        // max integer number of size sz
        upper_bounds.push_back((1 << sz) - 1);
      }
    }
  }

  std::vector<int> sample_once() {
    m_samples++;
    std::vector<int> sample;
    for (unsigned i = 0; i < m_vars.size(); i++) {
      if (should_fix[i]) {
        sample.push_back(lower_bounds[i]);
      } else {
        int range = upper_bounds[i] - lower_bounds[i] + 1;
        if (range <= 0) {
          sample.push_back(lower_bounds[i]);
          continue;
        }
        std::uniform_int_distribution<int> dist(0, range - 1);
        int output = lower_bounds[i] + dist(rng);
        sample.push_back(output);
      }
    }
    return sample;
  }

  bool check_random_model(std::vector<int> &assignments) {
    model rand_model(c);
    for (unsigned i = 0; i < m_vars.size(); i++) {
      z3::func_decl decl = m_vars[i].decl();
      z3::expr val_i = c.bv_val(assignments[i], m_vars[i].get_sort().bv_size());
      rand_model.add_const_interp(decl, val_i);
    }

    if (rand_model.eval(smt_formula, true).is_true()) {
      if (unique_models.size() == 0) {
        unique_models.push_back(assignments);
        return true;
      }
      bool is_unique = true;
      for (unsigned i = 0; i < unique_models.size(); i++) {
        std::vector<int> model = unique_models[i];
        bool same_model = true;
        for (unsigned j = 0; j < m_vars.size(); j++) {
          if (model[j] != assignments[j]) {
            same_model = false;
            break;
          }
        }
        if (same_model) {
          is_unique = false;
          break;
        }
      }
      if (is_unique) {
        m_unique++;
        unique_models.push_back(assignments);
      }
      return true;
    } else {
      return false;
    }
  }

  void run() {
    //        clock_gettime(CLOCK_REALTIME, &start_time);
    //        srand(start_time.tv_sec);
    // parse_cnf();
    for (auto &file : input_files) {
      reset_state();
      lower_bounds.clear();
      upper_bounds.clear();
      should_fix.clear();
      stop_requested = false;
      stop_reason.clear();
      input_file = file;
      parse_smt();
      if (!smt_formula) {
        log_error("Skipping file with parse failure: " + input_file);
        continue;
      }
      log_info("Parsed SMT input: " + input_file);

      m_vars = z3::expr_vector(c);
      get_expr_vars(smt_formula, m_vars);
      log_info("Collected variables; computing bounds");

      auto init = std::chrono::high_resolution_clock::now();
      //        struct timespec start;
      //        clock_gettime(CLOCK_REALTIME, &start);
      get_bounds();
      for (unsigned i = 0; i < m_vars.size(); i++) {
        if (lower_bounds[i] == upper_bounds[i]) {
          should_fix.push_back(true);
        } else {
          should_fix.push_back(false);
        }
      }
      log_info("Bounds computed; sampling models");

      auto finish = std::chrono::high_resolution_clock::now();
      //        struct timespec end;
      //
      //
      //        clock_gettime(CLOCK_REALTIME, &end);
      solver_time +=
          std::chrono::duration<double, std::milli>(finish - init).count();

      init = std::chrono::high_resolution_clock::now();
      for (int i = 0; i < max_samples; i++) {
        if (i % 5000 == 0)
          print_stats();
        if (stop_requested)
          break;
        //            struct timespec samp;
        //            clock_gettime(CLOCK_REALTIME, &samp);
        auto iter_start = std::chrono::high_resolution_clock::now();
        double elapsed =
            std::chrono::duration<double, std::milli>(iter_start - init)
                .count();
        if (elapsed >= max_time) {
          log_warn("Stopping: timeout");
          request_stop("timeout");
          break;
        }

        std::vector<int> sample = sample_once();
        if (check_random_model(sample)) {
          m_success++;
        }
        finish = std::chrono::high_resolution_clock::now();
        m_sample_time +=
            std::chrono::duration<double, std::milli>(finish - init).count();
      }
      if (stop_requested) {
        log_info("Stopped due to " + stop_reason);
      }
      print_stats();
    }
  }

  void request_stop(const std::string &reason) {
    stop_requested = true;
    stop_reason = reason;
  }

  void reset_state() {
    solver_time = 0;
    m_sample_time = 0;
    m_samples = 0;
    m_success = 0;
    unique_models.clear();
  }

  void print_stats() {

    std::cout << "solver time: " << solver_time << "\n";
    std::cout << "sample total time: " << m_sample_time << "\n";
    std::cout << "samples number: " << m_samples << "\n";
    std::cout << "samples success: " << m_success << "\n";
    std::cout << "unique models: " << unique_models.size() << "\n";
    std::cout << "------------------------------------------\n";

    std::ofstream of("res.log", std::ofstream::app);
    if (!of.is_open()) {
      log_warn("Failed to open res.log for append");
      return;
    }
    of << "solver time: " << solver_time << "\n";
    of << "sample total time: " << m_sample_time << "\n";
    of << "samples number: " << m_samples << "\n";
    of << "samples success: " << m_success << "\n";
    of << "unique models: " << unique_models.size() << "\n";
    of << "------------------------------------------\n";
    of.close();
  }
};
