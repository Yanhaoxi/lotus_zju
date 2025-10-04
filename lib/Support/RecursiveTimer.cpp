
#include "Support/RecursiveTimer.h"

static unsigned DepthOfTimeRecorder = 0;

static inline std::string Tab(unsigned N) {
  std::string Ret;
  while (N-- > 0)
    Ret.append("    ");
  return Ret;
}

RecursiveTimer::RecursiveTimer(const char *Prefix)
    : Begin(std::chrono::steady_clock::now()), Prefix(Prefix) {
  outs() << Tab(DepthOfTimeRecorder++) << Prefix << "...\n";
}

RecursiveTimer::RecursiveTimer(const std::string &Prefix)
    : Begin(std::chrono::steady_clock::now()), Prefix(Prefix) {
  outs() << Tab(DepthOfTimeRecorder++) << Prefix << "...\n";
}

RecursiveTimer::~RecursiveTimer() {
  std::chrono::steady_clock::time_point End = std::chrono::steady_clock::now();
  auto Milli =
      std::chrono::duration_cast<std::chrono::milliseconds>(End - Begin)
          .count();
  auto Time = Milli > 1000 ? Milli / 1000 : Milli;
  auto Unit = Milli > 1000 ? "s" : "ms";
  outs() << Tab(--DepthOfTimeRecorder) << Prefix << " takes " << Time << Unit
         << "!\n";
}

char RecursiveTimerPass::ID = 0;