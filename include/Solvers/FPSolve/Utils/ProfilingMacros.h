/**
 * Profiling macros for FPSolve
 */

#ifndef FPSOLVE_PROFILING_MACROS_H
#define FPSOLVE_PROFILING_MACROS_H

#ifdef OPCOUNT
#undef OPCOUNT
#include <iostream>
#define OPADD std::cout << "Addition on line: " << __LINE__ << " in file " << __FILE__ << std::endl
#define OPMULT std::cout << "Multiplication on line: " << __LINE__ << " in file " << __FILE__ << std::endl
#define OPSTAR std::cout << "Star on line: " << __LINE__ << " in file " << __FILE__ << std::endl
#else
#define OPADD
#define OPMULT
#define OPSTAR
#endif // OPCOUNT

#endif // FPSOLVE_PROFILING_MACROS_H

