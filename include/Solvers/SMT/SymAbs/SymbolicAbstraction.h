#pragma once

/**
 * @file SymbolicAbstraction.h
 * @brief Core algorithms for automatic abstraction of bit-vector formulae
 * 
 * This module implements algorithms from "Automatic Abstraction of Bit-Vector Formulae"
 * for computing symbolic abstractions using SMT solvers. The algorithms compute abstractions
 * in various domains: intervals, octagons, convex polyhedra, affine equalities, congruences,
 * and polynomials.
 * 
 * Key algorithms:
 * - Algorithm 7: α_lin-exp - Linear expression maximization
 * - Algorithm 8: α_oct^V - Octagonal abstraction
 * - Algorithm 9: α_conv^V - Convex polyhedral abstraction
 * - Algorithm 10: relax-conv - Relaxing convex polyhedra
 * - Algorithm 11: α_a-cong - Arithmetical congruence abstraction
 * - Algorithm 12: α_aff^V - Affine equality abstraction
 * - Algorithm 13: α_poly^V - Polynomial abstraction
 * 
 * This is a convenience header that includes all symbolic abstraction headers.
 * For better compile times, include only the specific headers you need:
 * - Solvers/SMT/SymAbs/Config.h - Configuration types
 * - Solvers/SMT/SymAbs/LinearExpression.h - Linear expression maximization
 * - Solvers/SMT/SymAbs/Octagon.h - Octagonal abstraction
 * - Solvers/SMT/SymAbs/Affine.h - Affine equality abstraction
 * - Solvers/SMT/SymAbs/Polyhedron.h - Convex polyhedral abstraction
 * - Solvers/SMT/SymAbs/Congruence.h - Congruence abstraction
 * - Solvers/SMT/SymAbs/Polynomial.h - Polynomial abstraction
 */

#include "Solvers/SMT/SymAbs/Config.h"
#include "Solvers/SMT/SymAbs/LinearExpression.h"
#include "Solvers/SMT/SymAbs/Octagon.h"
#include "Solvers/SMT/SymAbs/Affine.h"
#include "Solvers/SMT/SymAbs/Polyhedron.h"
#include "Solvers/SMT/SymAbs/Congruence.h"
#include "Solvers/SMT/SymAbs/Polynomial.h"
