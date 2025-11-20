set(CHECKER_GVFA_SOURCES
  GVFAVulnerabilityChecker.cpp
  CheckerUtils.cpp
  NullPointerChecker.cpp
  UseAfterFreeChecker.cpp
  UseOfUninitializedVariableChecker.cpp
  InvalidUseOfStackAddressChecker.cpp
  FreeOfNonHeapMemoryChecker.cpp
)

add_library(CheckerGVFA STATIC ${CHECKER_GVFA_SOURCES})
