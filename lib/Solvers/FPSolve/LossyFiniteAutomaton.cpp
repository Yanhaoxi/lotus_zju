/**
 * Lossy Finite Automaton implementation
 */

#ifdef HAVE_LIBFA

#include "Solvers/FPSolve/Semirings/LossyFiniteAutomaton.h"

namespace fpsolve {

LossyFiniteAutomaton LossyFiniteAutomaton::EMPTY = 
    LossyFiniteAutomaton(FiniteAutomaton());

LossyFiniteAutomaton LossyFiniteAutomaton::EPSILON = 
    LossyFiniteAutomaton(FiniteAutomaton::epsilon());

} // namespace fpsolve

#endif // HAVE_LIBFA

