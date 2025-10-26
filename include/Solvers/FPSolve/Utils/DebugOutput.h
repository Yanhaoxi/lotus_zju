/**
 * Debug output utilities for FPSolve
 */

#ifndef FPSOLVE_DEBUG_OUTPUT_H
#define FPSOLVE_DEBUG_OUTPUT_H

#include <iostream>
#include <string>

namespace fpsolve {

/*
 * Debugging output -- should be used for any information that helps in
 * debugging (i.e., it's ok if it slows things down).
 */

// Simple filename extraction without boost::filesystem dependency
inline const char* ExtractFilename(const char* path) {
  const char* filename = path;
  for (const char* p = path; *p; ++p) {
    if (*p == '/' || *p == '\\') {
      filename = p + 1;
    }
  }
  return filename;
}

#ifdef DEBUG_OUTPUT
#define DEBUG_LOCATION "(" << fpsolve::ExtractFilename(__FILE__) << ":" << __LINE__ << ")"
#define DMSG(msg) std::cerr << DEBUG_LOCATION << "  " << msg << std::endl;
#define DOUT(x) std::cerr << DEBUG_LOCATION << "  " << x;
#else
#define DMSG(msg) (void)0
#define DOUT(x) (void)0
#endif

} // namespace fpsolve

#endif // FPSOLVE_DEBUG_OUTPUT_H

