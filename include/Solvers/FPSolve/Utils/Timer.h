/**
 * Timer utility for FPSolve
 */

#ifndef FPSOLVE_TIMER_H
#define FPSOLVE_TIMER_H

#include <chrono>

namespace fpsolve {

class Timer {
  typedef std::chrono::high_resolution_clock clock;

  public:
    typedef std::chrono::microseconds Microseconds;
    typedef std::chrono::milliseconds Milliseconds;

    Timer() : start_(), stop_() {}

    void Start() {
      start_ = clock::now();
      stop_ = start_;
    }

    void Stop() {
      stop_ = clock::now();
    }

    Microseconds GetMicroseconds() const {
      return std::chrono::duration_cast<Microseconds>(stop_ - start_);
    }

    Milliseconds GetMilliseconds() const {
      return std::chrono::duration_cast<Milliseconds>(stop_ - start_);
    }

  private:
    clock::time_point start_;
    clock::time_point stop_;
};

} // namespace fpsolve

#endif // FPSOLVE_TIMER_H

