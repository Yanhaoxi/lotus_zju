/**
 * @file BugDetection.cpp
 * @brief Implementation of bug detection data structures
 */

#include "Analysis/llvmir-emul/BugDetection.h"
#include <llvm/Support/raw_ostream.h>
#include <sstream>

namespace miri {

std::string DetectedBug::getDetailedMessage() const {
    std::ostringstream oss;
    oss << message;
    
    // Add concrete values if available
    if (!context.concrete_values.empty()) {
        oss << "\n  Concrete values:";
        for (const auto& [name, value] : context.concrete_values) {
            oss << "\n    " << name << " = " << value 
                << " (0x" << std::hex << value << std::dec << ")";
        }
    }
    
    // Add memory access information
    if (context.access_addr != 0) {
        oss << "\n  Access: address=0x" << std::hex << context.access_addr
            << ", size=" << std::dec << context.access_size
            << ", " << (context.is_write ? "write" : "read");
    }
    
    // Add memory region information
    if (context.region_base != 0) {
        oss << "\n  Region: [0x" << std::hex << context.region_base
            << ", 0x" << (context.region_base + context.region_size)
            << std::dec << "), size=" << context.region_size;
    }
    
    // Add allocation site
    if (context.alloc_site) {
        oss << "\n  Allocated at: ";
        std::string alloc_str;
        llvm::raw_string_ostream alloc_os(alloc_str);
        context.alloc_site->print(alloc_os);
        oss << alloc_os.str();
    }
    
    // Add free site (for use-after-free/double-free)
    if (context.free_site) {
        oss << "\n  Freed at: ";
        std::string free_str;
        llvm::raw_string_ostream free_os(free_str);
        context.free_site->print(free_os);
        oss << free_os.str();
    }
    
    // Add additional info
    if (!context.additional_info.empty()) {
        oss << "\n  " << context.additional_info;
    }
    
    // Add call stack
    if (!context.call_stack.empty()) {
        oss << "\n  Call stack:";
        for (auto* func : context.call_stack) {
            oss << "\n    " << func->getName().str();
        }
    }
    
    return oss.str();
}

} // namespace miri

