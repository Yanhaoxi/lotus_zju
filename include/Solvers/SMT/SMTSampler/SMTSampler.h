/**
 * @file SMTSampler.h
 * @brief Main header for SMT sampling functionality
 *
 * This file provides the common includes and forward declarations for the SMT sampling module.
 * The module implements various techniques for sampling satisfying assignments (models)
 * from SMT formulas, including:
 *
 * 1. QuickSampler: A mutation-based approach for generating diverse models.
 * 2. RegionSampler (PolySampler): A geometry-based approach for sampling from convex polytopes defined by linear constraints.
 * 3. IntervalSampler: An interval-based sampling strategy.
 *
 * These samplers are used for test case generation, solution space exploration, and
 * analyzing formula sensitivity.
 */

#pragma once

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
//#include <string.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <z3++.h>

#include "Solvers/SMT/LIBSMT/Z3Plus.h"

using namespace std;
using namespace z3;

// Forward declarations for sampler implementations
class quick_sampler;
struct interval_sampler;
struct region_sampler;
