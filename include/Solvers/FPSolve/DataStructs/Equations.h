/**
 * Equations data structures for FPSolve
 */

#ifndef FPSOLVE_EQUATIONS_H
#define FPSOLVE_EQUATIONS_H

#include "Solvers/FPSolve/DataStructs/Var.h"
#include <utility>
#include <vector>

namespace fpsolve {

// Forward declarations
template <typename SR> class CommutativePolynomial;
template <typename SR> class NonCommutativePolynomial;

// Generic equations: vector of (VarId, Polynomial) pairs
template<template <typename> class Poly, typename SR>
using GenericEquations = std::vector<std::pair<VarId, Poly<SR>>>;

// Specialization for commutative polynomials
template <typename A>
using Equations = GenericEquations<CommutativePolynomial, A>;

// Specialization for non-commutative polynomials  
template <typename A>
using NCEquations = GenericEquations<NonCommutativePolynomial, A>;

// Transform equations by mapping semiring elements
template <template <typename> class Poly, typename SR, typename F>
auto MapEquations(const GenericEquations<Poly, SR> &equations, F fun)
    -> GenericEquations<Poly, typename std::result_of<F(SR)>::type> {

  GenericEquations<Poly, typename std::result_of<F(SR)>::type> new_equations;

  for (auto &var_poly : equations) {
    new_equations.emplace_back(var_poly.first,
      var_poly.second.Map([&fun](const SR &coeff) {
        return fun(coeff);
      })
    );
  }

  return new_equations;
}

} // namespace fpsolve

#endif // FPSOLVE_EQUATIONS_H

