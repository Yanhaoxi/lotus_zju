/**
 * @file BugCheckerFactory.cpp
 * @brief Implementation of bug checker factory
 */

#include "Analysis/llvmir-emul/BugCheckers.h"
#include <algorithm>

namespace miri {

std::vector<std::unique_ptr<BugChecker>> BugCheckerFactory::createAllCheckers() {
    std::vector<std::unique_ptr<BugChecker>> checkers;
    
    checkers.push_back(std::make_unique<MemorySafetyChecker>());
    checkers.push_back(std::make_unique<UninitializedMemoryChecker>());
    checkers.push_back(std::make_unique<DivisionByZeroChecker>());
    checkers.push_back(std::make_unique<IntegerOverflowChecker>());
    checkers.push_back(std::make_unique<InvalidShiftChecker>());
    checkers.push_back(std::make_unique<NullPointerChecker>());
    
    return checkers;
}

std::unique_ptr<BugChecker> BugCheckerFactory::createChecker(const std::string& name) {
    std::string lowerName = name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    
    if (lowerName == "memorysafety" || lowerName == "memory") {
        return std::make_unique<MemorySafetyChecker>();
    } else if (lowerName == "uninitializedmemory" || lowerName == "uninitialized") {
        return std::make_unique<UninitializedMemoryChecker>();
    } else if (lowerName == "divisionbyzero" || lowerName == "divzero") {
        return std::make_unique<DivisionByZeroChecker>();
    } else if (lowerName == "integeroverflow" || lowerName == "overflow") {
        return std::make_unique<IntegerOverflowChecker>();
    } else if (lowerName == "invalidshift" || lowerName == "shift") {
        return std::make_unique<InvalidShiftChecker>();
    } else if (lowerName == "nullpointer" || lowerName == "nullptr") {
        return std::make_unique<NullPointerChecker>();
    }
    
    return nullptr;
}

std::vector<std::string> BugCheckerFactory::getAvailableCheckers() {
    return {
        "MemorySafety",
        "UninitializedMemory",
        "DivisionByZero",
        "IntegerOverflow",
        "InvalidShift",
        "NullPointer"
    };
}

} // namespace miri

