/**
 * Grammar Parser for FPSolve
 */

#ifndef FPSOLVE_PARSER_H
#define FPSOLVE_PARSER_H

#include <string>
#include <utility>
#include <vector>

#include "Solvers/FPSolve/DataStructs/Var.h"
#include "Solvers/FPSolve/DataStructs/Equations.h"
#include "Solvers/FPSolve/Polynomials/CommutativePolynomial.h"
#include "Solvers/FPSolve/Polynomials/NonCommutativePolynomial.h"
#include "Solvers/FPSolve/Semirings/CommutativeRExp.h"
#include "Solvers/FPSolve/Semirings/FreeSemiring.h"

#ifdef USE_GENEPI
#include "Solvers/FPSolve/Semirings/SemilinSetNdd.h"
#endif

namespace fpsolve {

// Forward declarations
template <typename SR>
class CommutativePolynomial;

template <typename SR>
class NonCommutativePolynomial;

class CommutativeRExp;
class FreeSemiring;

/**
 * Parser for FPSolve equation systems
 * 
 * Supports parsing different semiring types:
 * - CommutativeRExp: Regular expressions
 * - FreeSemiring: Free symbolic expressions
 * - SemilinSetNdd: Semilinear sets with NDDs (if USE_GENEPI is defined)
 * - PrefixSemiring: Prefix sequences
 */
class Parser
{
public:
  Parser();
  
  /**
   * Parse equations over commutative regular expressions
   * Input format: <X> ::= "a" <Y> | "b";
   */
  std::vector<std::pair<VarId, CommutativePolynomial<CommutativeRExp>>> 
  rexp_parser(std::string input);

#ifdef USE_GENEPI
  /**
   * Parse equations over semilinear sets using NDDs
   * Input format: <X> ::= "<a:1, b:2>" <Y> | "<>";
   */
  std::vector<std::pair<VarId, CommutativePolynomial<SemilinSetNdd>>> 
  slsetndd_parser(std::string input);
#endif

  /**
   * Parse equations over free semiring
   * Input format: <X> ::= a <Y><Y> | c;
   */
  std::vector<std::pair<VarId, NonCommutativePolynomial<FreeSemiring>>> 
  free_parser(std::string input);


#ifdef HAVE_PREFIX_SEMIRING
  /**
   * Parse equations over prefix semiring
   * Input format: <X> ::= "a,b," <Y> | "c,";
   * @param input The grammar string
   * @param length Maximum prefix length
   * Note: Requires HAVE_PREFIX_SEMIRING to be defined
   */
  std::vector<std::pair<VarId, NonCommutativePolynomial<PrefixSemiring>>> 
  prefix_parser(std::string input, unsigned int length);
#endif
};

} // namespace fpsolve

#endif // FPSOLVE_PARSER_H
