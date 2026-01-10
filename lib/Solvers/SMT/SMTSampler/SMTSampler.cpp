/**
 * @file SMTSampler.cpp
 * @brief Implementation of sampling techniques for SMT formulas
 *
 * This file implements two approaches for sampling satisfying models from SMT
 * formulas:
 * 1. quick_sampler: A mutation-based approach that generates diverse models by
 * flipping variable assignments and exploring the solution space
 * 2. region_sampler: A bounds-based approach that samples models by determining
 * variable bounds and randomly selecting values within those bounds
 *
 * These sampling techniques are useful for:
 * - Test case generation
 * - Exploring the solution space of constraints
 * - Finding diverse satisfying assignments
 * - Analyzing the sensitivity of formulas to variable changes
 *
 * The implementation focuses on efficiency and diversity of the generated
 * models.
 *
 * NOTE: The concrete implementations have been moved to:
 * - QuickSampler.cpp (quick_sampler class)
 * - RegionSampler.cpp (region_sampler struct)
 */

#include "Solvers/SMT/SMTSampler/SMTSampler.h"
