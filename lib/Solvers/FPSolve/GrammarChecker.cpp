/**
 * Grammar Equivalence Checker
 * 
 * Checks whether grammars generate the same language modulo commutativity
 * or subword closure (lossy approximation).
 */

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <numeric>

// Uncomment if boost::program_options is available
// #include <boost/program_options.hpp>

#include "Solvers/FPSolve/DataStructs/Matrix.h"
#include "Solvers/FPSolve/DataStructs/Equations.h"
#include "Solvers/FPSolve/Polynomials/CommutativePolynomial.h"
#include "Solvers/FPSolve/Polynomials/NonCommutativePolynomial.h"

#ifdef USE_LIBFA
#include "Solvers/FPSolve/Polynomials/LossyNonCommutativePolynomial.h"
#endif

#include "Solvers/FPSolve/Semirings/FreeSemiring.h"

#ifdef USE_GENEPI
#include "Solvers/FPSolve/Semirings/SemilinSetNdd.h"
#endif

#include "Solvers/FPSolve/Parser/Parser.h"
#include "Solvers/FPSolve/Solvers/NewtonGeneric.h"
#include "Solvers/FPSolve/Solvers/SolverUtils.h"
#include "Solvers/FPSolve/Utils/StringUtil.h"
#include "Solvers/FPSolve/Utils/Timer.h"

namespace fpsolve {

/**
 * Check whether a set of grammars generates the same language up to commutativity
 * (and modulo additional overapproximations given by the semiring)
 */
template <typename SR>
void check_all_equal_commutative(const std::string& startsymbol, 
                                  const std::vector<std::string>& inputs) {

  Parser p;
  int num_grammars = inputs.size();

  auto nc_equations = p.free_parser(inputs[0]);

  std::cout << "Eq (non-comm) : " << '\n';
  PrintEquations(nc_equations);

  // Use appropriate semiring (has to be commutative!)
  auto equations_fst = MakeCommEquationsAndMap(nc_equations, 
    [](const FreeSemiring &c) -> SR {
      auto srconv = SRConverter<SR>();
      return c.Eval(srconv);
    });

  std::cout << "Eq (comm) : "  << '\n';
  PrintEquations(equations_fst);

  Timer timer;
  timer.Start();

  ValuationMap<SR> sol_fst = apply_solver<NewtonCL, CommutativePolynomial>(
    equations_fst, true, false, 0, false);

  bool all_equal = true;
  for(int i=1; i<num_grammars; i++) {
    auto equations = MakeCommEquationsAndMap(p.free_parser(inputs[i]), 
      [](const FreeSemiring &c) -> SR {
        auto srconv = SRConverter<SR>();
        return c.Eval(srconv);
      });

    ValuationMap<SR> sol = apply_solver<NewtonCL, CommutativePolynomial>(
      equations, true, false, 0, false);

    if(startsymbol.compare("") == 0) {
      if(sol[equations[0].first] != sol_fst[equations_fst[0].first]) {
        std::cout << "[DIFF] Difference found for startsymbols (" 
                  << equations_fst[0].first << "," << equations[0].first << ")" << std::endl;
        std::cout << "0:" << result_string(sol_fst) << std::endl 
                  << i << ":" << result_string(sol) << std::endl;
        all_equal = false;
        break;
      }
    }
    else {
      if(sol.find(Var::GetVarId(startsymbol)) == sol.end() || 
         sol_fst.find(Var::GetVarId(startsymbol)) == sol_fst.end()) {
        std::cout << "[ERROR] startsymbol (" << startsymbol << ") does not occur!"<< '\n';
        return;
      }
      else if(sol[Var::GetVarId(startsymbol)] != sol_fst[Var::GetVarId(startsymbol)]) {
        std::cout << "[DIFF] Difference found for startsymbol (" << startsymbol << ")" << '\n' 
                  << "0:" << result_string(sol_fst)
                  << std::endl << i << ":" << result_string(sol) << std::endl;
        all_equal = false;
        break;
      }
    }
  }

  if(all_equal) {
    std::cout << "[EQUIV] All grammars equivalent modulo commutativity" << '\n';
  }

  timer.Stop();
  std::cout
  << "Total checking time:\t" << timer.GetMilliseconds().count()
  << " ms" << " ("
  << timer.GetMicroseconds().count()
  << "us)" << '\n';
}

#ifdef USE_LIBFA
/**
 * Check equality using lossy approximation (subword closure)
 */
void check_all_equal_lossy(const std::string& startsymbol, 
                            const std::vector<std::string>& inputs, 
                            int refinementDepth) {
  int num_grammars = inputs.size();
  Parser p;
  
  auto eq_tmp = MapEquations(p.free_parser(inputs[0]), 
    [](const FreeSemiring &c) -> LossyFiniteAutomaton {
      auto srconv = SRConverter<LossyFiniteAutomaton>();
      return c.Eval(srconv);
    });

  auto equations_fst = NCEquationsBase<LossyFiniteAutomaton>(
    eq_tmp.begin(), eq_tmp.end());

  VarId S_1;
  if(startsymbol.compare("") == 0) {
    S_1 = equations_fst[0].first;
  } else {
    S_1 = Var::GetVarId(startsymbol);
  }

  Timer timer;
  timer.Start();

  bool all_equal = true;
  for(int i=1; i<num_grammars; i++) {
    auto eq_tmp2 = MapEquations(p.free_parser(inputs[i]), 
      [](const FreeSemiring &c) -> LossyFiniteAutomaton {
        auto srconv = SRConverter<LossyFiniteAutomaton>();
        return c.Eval(srconv);
      });
    auto equations = NCEquationsBase<LossyFiniteAutomaton>(
      eq_tmp2.begin(), eq_tmp2.end());

    VarId S_2;
    if(startsymbol.compare("") == 0) {
      S_2 = equations[0].first;
    } else {
      S_2 = Var::GetVarId(startsymbol);
    }

    auto witness = NonCommutativePolynomial<LossyFiniteAutomaton>::refineCourcelle(
      equations_fst, S_1, equations, S_1, refinementDepth);

    if(witness != LossyFiniteAutomaton::null()) {
      if(startsymbol.compare("") == 0) {
        std::cout << "[DIFF] Difference found for startsymbols (" 
                  << equations_fst[0].first << "," << equations[0].first << ")" << std::endl;
      }
      else {
        std::cout << "[DIFF] Difference found for startsymbols (" 
                  << S_1 << "," << S_2 << ")" << std::endl;
      }
      std::cout << "Witness: " << witness.string() << std::endl;
      all_equal = false;
      break;
    }
  }

  if(all_equal) {
    std::cout << "[EQUIV] All grammars equivalent modulo subword-closure" << std::endl;
  }

  timer.Stop();
  std::cout
  << "Total checking time:\t" << timer.GetMilliseconds().count()
  << " ms" << " ("
  << timer.GetMicroseconds().count()
  << "us)" << std::endl;
}
#endif

/**
 * Example grammar checker main function
 * 
 * Tests whether two grammars generate the same language modulo commutativity.
 * We use semilinear sets in constant-period representation to represent Parikh images
 * and check their equivalence via NDDs.
 * 
 * Note: This is a reference implementation. Adapt for your use case.
 */
int grammar_checker_main(int argc, char* argv[]) {
  
  std::cout << "FPSolve Grammar Equivalence Checker" << '\n';
  std::cout << "====================================" << '\n';
  std::cout << '\n';
  std::cout << "This is a reference implementation for checking grammar equivalence." << '\n';
  std::cout << "For command-line options, refer to FPsolve-master/c/src/gr_checker.cpp" << '\n';
  std::cout << '\n';
  
  // Example usage would go here
  // In a real application, parse command-line arguments and call
  // check_all_equal_commutative() or check_all_equal_lossy()
  
  std::cout << "Usage example:" << '\n';
  std::cout << "  - Prepare grammar files in FPSolve format" << '\n';
  std::cout << "  - Call check_all_equal_commutative<SemilinearSetL>(startsymbol, inputs)" << '\n';
  std::cout << "  - Or call check_all_equal_lossy(startsymbol, inputs, refinement_depth)" << '\n';
  
  return EXIT_SUCCESS;
}

} // namespace fpsolve

// Uncomment if you want to build this as a standalone executable
// int main(int argc, char* argv[]) {
//   return fpsolve::grammar_checker_main(argc, argv);
// }

