/**
 * Finite Automaton wrapper for libfa
 * 
 * NOTE: This requires libfa to be installed.
 * On Ubuntu: sudo apt-get install libfa-dev
 * Or build from source: http://augeas.net/libfa/
 */

#ifndef FPSOLVE_FINITE_AUTOMATON_H
#define FPSOLVE_FINITE_AUTOMATON_H

#ifdef HAVE_LIBFA

extern "C" {
    #include <fa.h>
}

#include <string>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <cassert>

namespace fpsolve {

class FiniteAutomaton {
private:
  struct fa* automaton;
  int epsilon_closed;

  FiniteAutomaton(struct fa* fa_ptr) : automaton(fa_ptr), epsilon_closed(0) {}

public:
  // Empty language automaton
  FiniteAutomaton() {
    automaton = fa_make_basic(FA_EMPTY);
    epsilon_closed = 0;
  }

  // Build from POSIX regular expression
  FiniteAutomaton(const std::string& regularExpression) {
    fa_compile(regularExpression.c_str(), regularExpression.size(), &automaton);
    epsilon_closed = 0;
    assert(automaton != NULL);
  }

  // Copy constructor
  FiniteAutomaton(const FiniteAutomaton& other) {
    automaton = fa_clone(other.automaton);
    epsilon_closed = other.epsilon_closed;
  }

  // Move constructor
  FiniteAutomaton(FiniteAutomaton&& other) noexcept
      : automaton(other.automaton), epsilon_closed(other.epsilon_closed) {
    other.automaton = nullptr;
  }

  ~FiniteAutomaton() {
    if (automaton) {
      fa_free(automaton);
    }
  }

  // Assignment operators
  FiniteAutomaton& operator=(const FiniteAutomaton& other) {
    if (this != &other) {
      if (automaton) fa_free(automaton);
      automaton = fa_clone(other.automaton);
      epsilon_closed = other.epsilon_closed;
    }
    return *this;
  }

  FiniteAutomaton& operator=(FiniteAutomaton&& other) noexcept {
    if (this != &other) {
      if (automaton) fa_free(automaton);
      automaton = other.automaton;
      epsilon_closed = other.epsilon_closed;
      other.automaton = nullptr;
    }
    return *this;
  }

  // Epsilon automaton
  static FiniteAutomaton epsilon() {
    return FiniteAutomaton(fa_make_basic(FA_EPSILON));
  }

  // Operations
  FiniteAutomaton concat(const FiniteAutomaton& other) const {
    return FiniteAutomaton(fa_concat(automaton, other.automaton));
  }

  FiniteAutomaton union_op(const FiniteAutomaton& other) const {
    return FiniteAutomaton(fa_union(automaton, other.automaton));
  }

  FiniteAutomaton star() const {
    return FiniteAutomaton(fa_iter(automaton, 0, -1));
  }

  FiniteAutomaton minimize() const {
    struct fa* minimized = fa_clone(automaton);
    fa_minimize(minimized);
    return FiniteAutomaton(minimized);
  }

  FiniteAutomaton epsilonClosure() const {
    struct fa* closed = fa_clone(automaton);
    fa_as_regexp(closed, nullptr, nullptr); // This computes epsilon closure
    return FiniteAutomaton(closed);
  }

  bool empty() const {
    return fa_is_basic(automaton, FA_EMPTY);
  }

  bool contains_epsilon() const {
    return fa_contains(automaton, "", 0);
  }

  bool operator==(const FiniteAutomaton& other) const {
    return fa_equals(automaton, other.automaton);
  }

  bool operator!=(const FiniteAutomaton& other) const {
    return !(*this == other);
  }

  size_t size() const {
    size_t count = 0;
    for (struct state* s = automaton->initial; s != nullptr; s = s->next) {
      count++;
    }
    return count;
  }

  std::string to_regexp() const {
    char* regexp_str;
    size_t len;
    fa_as_regexp(automaton, &regexp_str, &len);
    std::string result(regexp_str, len);
    free(regexp_str);
    return result;
  }

  struct fa* get_fa() const { return automaton; }
};

} // namespace fpsolve

#endif // HAVE_LIBFA

#endif // FPSOLVE_FINITE_AUTOMATON_H

