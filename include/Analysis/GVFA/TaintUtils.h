#ifndef GVFA_TAINT_UTILS_H
#define GVFA_TAINT_UTILS_H

#include <string>
#include <set>

namespace gvfa {

// Simple taint utility functions for GVFA
// These are minimal implementations of the functions that GVFA actually uses
class TaintUtils {
public:
    // Taint source/sink detection utilities
    static bool isKnownSourceFunction(const std::string& funcName);
    static bool isKnownSinkFunction(const std::string& funcName);
    static bool isKnownSanitizerFunction(const std::string& funcName);
};

} // namespace gvfa

#endif // GVFA_TAINT_UTILS_H
