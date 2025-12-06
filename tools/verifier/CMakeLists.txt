# Verifier umbrella CMake for tools under tools/verifier
#
# Exposes toggles for the various verifier toolchains and delegates
# to their existing CMake configurations within this directory.
#
# This file is intended to be included from the top-level with:
#   add_subdirectory(tools/verifier)
#
# Available subprojects:
#   - symbolic_abstraction      (BUILD_SYMBOLIC_ABSTRACTION)
#   - clam          (ENABLE_CLAM defined at top-level)
#   - crabber       (ENABLE_CLAM defined at top-level, requires CRAB)
#   - horn-ice      (BUILD_HORN_ICE)
#   - seahorn       (ENABLE_SEAHORN, optional)

# SymbolicAbstraction
option(BUILD_SYMBOLIC_ABSTRACTION "Build SymbolicAbstraction verifier tools" ON)
if(BUILD_SYMBOLIC_ABSTRACTION)
    add_subdirectory(symbolic-abstraction)
endif()

# CLAM tools (clam, clam-diff, clam-pp)
if(ENABLE_CLAM)
    add_subdirectory(clam)
endif()

# Seahorn tools (optional)
if(ENABLE_SEAHORN)
    add_subdirectory(seahorn)
endif()

# horn-ice
option(BUILD_HORN_ICE "Build ICE learning for CHC and Boogie" OFF)
if(BUILD_HORN_ICE)
    add_subdirectory(horn-ice)
endif()


