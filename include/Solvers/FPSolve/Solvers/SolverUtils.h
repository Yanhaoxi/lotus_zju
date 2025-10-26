/**
 * Solver Utilities - SCC decomposition and equation grouping
 */

#ifndef FPSOLVE_SOLVER_UTILS_H
#define FPSOLVE_SOLVER_UTILS_H

#include "Solvers/FPSolve/DataStructs/Var.h"
#include "Solvers/FPSolve/DataStructs/Equations.h"
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <stack>

namespace fpsolve {

/**
 * Strongly Connected Components decomposition using Tarjan's algorithm
 */
template <typename SR, template <typename> class Poly>
class SCCDecomposer {
private:
  struct SCCInfo {
    int index;
    int lowlink;
    bool on_stack;
  };

  std::unordered_map<VarId, SCCInfo> info_;
  std::stack<VarId> stack_;
  int index_;
  std::vector<std::vector<VarId>> sccs_;

  void strongconnect(VarId v,
                     const std::unordered_map<VarId, std::vector<VarId>>& adj) {
    info_[v].index = index_;
    info_[v].lowlink = index_;
    info_[v].on_stack = true;
    index_++;
    stack_.push(v);

    auto it = adj.find(v);
    if (it != adj.end()) {
      for (const auto& w : it->second) {
        if (info_[w].index == -1) {
          strongconnect(w, adj);
          info_[v].lowlink = std::min(info_[v].lowlink, info_[w].lowlink);
        } else if (info_[w].on_stack) {
          info_[v].lowlink = std::min(info_[v].lowlink, info_[w].index);
        }
      }
    }

    if (info_[v].lowlink == info_[v].index) {
      std::vector<VarId> scc;
      VarId w;
      do {
        w = stack_.top();
        stack_.pop();
        info_[w].on_stack = false;
        scc.push_back(w);
      } while (w != v);
      sccs_.push_back(scc);
    }
  }

public:
  std::vector<std::vector<VarId>> decompose(
      const GenericEquations<Poly, SR>& equations) {
    
    // Build adjacency list
    std::unordered_map<VarId, std::vector<VarId>> adj;
    
    for (const auto& eq : equations) {
      VarId lhs = eq.first;
      auto vars = eq.second.get_variables();
      adj[lhs] = vars;
      
      // Initialize info for all variables
      if (info_.find(lhs) == info_.end()) {
        info_[lhs] = {-1, -1, false};
      }
      for (const auto& v : vars) {
        if (info_.find(v) == info_.end()) {
          info_[v] = {-1, -1, false};
        }
      }
    }

    index_ = 0;
    sccs_.clear();

    // Run Tarjan's algorithm
    for (const auto& eq : equations) {
      if (info_[eq.first].index == -1) {
        strongconnect(eq.first, adj);
      }
    }

    // Reverse to get bottom-up order
    std::reverse(sccs_.begin(), sccs_.end());
    return sccs_;
  }
};

/**
 * Group equations by strongly connected components
 */
template <typename SR, template <typename> class Poly>
std::vector<GenericEquations<Poly, SR>> group_by_scc(
    const GenericEquations<Poly, SR>& equations) {
  
  SCCDecomposer<SR, Poly> decomposer;
  auto sccs = decomposer.decompose(equations);

  // Create equation map
  std::unordered_map<VarId, Poly<SR>> eq_map;
  for (const auto& eq : equations) {
    eq_map[eq.first] = eq.second;
  }

  // Group equations by SCC
  std::vector<GenericEquations<Poly, SR>> grouped;
  for (const auto& scc : sccs) {
    GenericEquations<Poly, SR> group;
    for (const auto& var : scc) {
      auto it = eq_map.find(var);
      if (it != eq_map.end()) {
        group.emplace_back(var, it->second);
      }
    }
    if (!group.empty()) {
      grouped.push_back(group);
    }
  }

  return grouped;
}

/**
 * Apply solver with SCC decomposition
 */
template <template <typename> class SolverType,
          template <typename> class Poly,
          typename SR>
ValuationMap<SR> apply_solver_with_scc(
    const GenericEquations<Poly, SR>& equations,
    std::size_t max_iterations) {
  
  // Group by SCC
  auto scc_groups = group_by_scc(equations);

  // Solve each SCC in order
  ValuationMap<SR> solution;

  for (const auto& group : scc_groups) {
    // Substitute already solved variables
    GenericEquations<Poly, SR> simplified_group;
    for (const auto& eq : group) {
      auto simplified = eq.second.partial_eval(solution);
      simplified_group.emplace_back(eq.first, simplified);
    }

    // Solve this SCC
    SolverType<SR> solver;
    
    // Dynamic iteration count based on group size
    std::size_t iterations = std::max(simplified_group.size() + 1, max_iterations);
    
    auto group_solution = solver.solve_fixpoint(simplified_group, iterations);

    // Merge into overall solution
    solution.insert(group_solution.begin(), group_solution.end());
  }

  return solution;
}

/**
 * Timer utility
 */
class Timer {
private:
  std::chrono::high_resolution_clock::time_point start_;
  std::chrono::high_resolution_clock::time_point end_;

public:
  void Start() {
    start_ = std::chrono::high_resolution_clock::now();
  }

  void Stop() {
    end_ = std::chrono::high_resolution_clock::now();
  }

  std::chrono::milliseconds GetMilliseconds() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(end_ - start_);
  }

  std::chrono::microseconds GetMicroseconds() const {
    return std::chrono::duration_cast<std::chrono::microseconds>(end_ - start_);
  }
};

} // namespace fpsolve

#endif // FPSOLVE_SOLVER_UTILS_H

