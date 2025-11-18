/**
 * @file MemoryModel.cpp
 * @brief Implementation of enhanced memory model

 */

#include "Analysis/llvmir-emul/MemoryModel.h"
#include <llvm/IR/GlobalVariable.h>
#include <llvm/Support/raw_ostream.h>
#include <algorithm>
#include <sstream>

namespace miri {

//=============================================================================
// MemoryRegion Implementation
//=============================================================================

bool MemoryRegion::isInitialized(uint64_t addr, size_t len) const {
    if (!contains(addr) || !isValidAccess(addr, len)) {
        return false;
    }
    
    uint64_t offset = addr - base_address;
    for (size_t i = 0; i < len; ++i) {
        if (offset + i >= initialized_bytes.size()) {
            return false;
        }
        if (!initialized_bytes[offset + i]) {
            return false;
        }
    }
    return true;
}

void MemoryRegion::markInitialized(uint64_t addr, size_t len) {
    if (!contains(addr)) {
        return;
    }
    
    uint64_t offset = addr - base_address;
    size_t bytes_to_mark = std::min(len, static_cast<size_t>(size - offset));
    
    for (size_t i = 0; i < bytes_to_mark; ++i) {
        if (offset + i < initialized_bytes.size()) {
            initialized_bytes[offset + i] = true;
        }
    }
}

void MemoryRegion::dump(llvm::raw_ostream& OS) const {
    OS << "MemoryRegion[0x";
    OS.write_hex(base_address);
    OS << ", size=" << size << ", ";
    
    switch (state) {
        case MemoryState::Unallocated: OS << "unallocated"; break;
        case MemoryState::Allocated: OS << "allocated"; break;
        case MemoryState::Freed: OS << "freed"; break;
    }
    
    OS << ", type=";
    switch (alloc_type) {
        case AllocationType::Stack: OS << "stack"; break;
        case AllocationType::Heap: OS << "heap"; break;
        case AllocationType::Global: OS << "global"; break;
        case AllocationType::Unknown: OS << "unknown"; break;
    }
    
    if (alloc_site) {
        OS << ", alloc=";
        alloc_site->print(OS);
    }
    
    if (free_site) {
        OS << ", freed=";
        free_site->print(OS);
    }
    
    OS << "]\n";
}

//=============================================================================
// CheckResult Implementation
//=============================================================================

const char* CheckResult::getStatusString() const {
    switch (status) {
        case Status::OK: return "OK";
        case Status::OutOfBounds: return "Out of Bounds";
        case Status::UseAfterFree: return "Use After Free";
        case Status::UninitializedRead: return "Uninitialized Read";
        case Status::NullPointerDeref: return "Null Pointer Dereference";
        case Status::InvalidPointer: return "Invalid Pointer";
        case Status::DoubleFree: return "Double Free";
        default: return "Unknown";
    }
}

//=============================================================================
// MemoryModel Implementation
//=============================================================================

bool MemoryModel::registerAllocation(uint64_t base, size_t size,
                                     llvm::Instruction* alloc_site,
                                     AllocationType type) {
    // Check for overlaps with existing allocations
    for (const auto& pair : regions_) {
        uint64_t existing_base = pair.first;
        const MemoryRegion& region = pair.second;
        if (region.getState() == MemoryState::Allocated) {
            uint64_t existing_end = existing_base + region.getSize();
            uint64_t new_end = base + size;
            
            // Check for overlap
            if (!(new_end <= existing_base || base >= existing_end)) {
                // Overlap detected
                return false;
            }
        }
    }
    
    // Register the new region
    regions_.emplace(base, MemoryRegion(base, size, alloc_site, type));
    updateAddressMapping(base, size);
    
    return true;
}

bool MemoryModel::registerStackAllocation(uint64_t base, size_t size,
                                          llvm::AllocaInst* alloc_site) {
    bool success = registerAllocation(base, size, alloc_site, AllocationType::Stack);
    
    if (success) {
        // Track this allocation for stack frame cleanup
        if (stack_frames_.size() <= stack_depth_) {
            stack_frames_.resize(stack_depth_ + 1);
        }
        stack_frames_[stack_depth_].insert(base);
    }
    
    return success;
}

bool MemoryModel::registerHeapAllocation(uint64_t base, size_t size,
                                         llvm::Instruction* alloc_site,
                                         bool zero_initialized) {
    bool success = registerAllocation(base, size, alloc_site, AllocationType::Heap);
    
    if (success && zero_initialized) {
        // Mark all bytes as initialized (for calloc)
        auto* region = findRegionByBase(base);
        if (region) {
            region->markFullyInitialized();
        }
    }
    
    return success;
}

bool MemoryModel::registerGlobalVariable(uint64_t base, size_t size,
                                         llvm::GlobalVariable* gv) {
    // Global variables are always initialized
    bool success = registerAllocation(base, size, 
                                     nullptr,  // No specific instruction
                                     AllocationType::Global);
    
    if (success) {
        auto* region = findRegionByBase(base);
        if (region) {
            region->markFullyInitialized();
        }
    }
    
    return success;
}

CheckResult MemoryModel::markFreed(uint64_t addr, llvm::Instruction* free_site) {
    // Check for null pointer
    if (isNullPointer(addr)) {
        return CheckResult(CheckResult::Status::NullPointerDeref, nullptr,
                          "Attempting to free null pointer");
    }
    
    // Find the region
    MemoryRegion* region = findRegion(addr);
    
    if (!region) {
        return CheckResult(CheckResult::Status::InvalidPointer, nullptr,
                          "Attempting to free invalid pointer (not in any allocation)");
    }
    
    // Check if freeing from the base address (required for free())
    if (addr != region->getBase()) {
        std::ostringstream oss;
        oss << "Attempting to free pointer not at allocation base (base=0x";
        oss << std::hex << region->getBase() << std::dec;
        oss << ", freed=0x";
        oss << std::hex << addr << std::dec << ")";
        return CheckResult(CheckResult::Status::InvalidPointer, region, oss.str());
    }
    
    // Check if already freed
    if (region->getState() == MemoryState::Freed) {
        return CheckResult(CheckResult::Status::DoubleFree, region,
                          "Attempting to free already-freed memory");
    }
    
    // Check allocation type - can't free stack or global memory
    if (region->getAllocType() == AllocationType::Stack) {
        return CheckResult(CheckResult::Status::InvalidPointer, region,
                          "Attempting to free stack memory");
    }
    
    if (region->getAllocType() == AllocationType::Global) {
        return CheckResult(CheckResult::Status::InvalidPointer, region,
                          "Attempting to free global memory");
    }
    
    // Valid free - mark as freed
    region->markFreed(free_site);
    
    return CheckResult(CheckResult::Status::OK, region, "");
}

void MemoryModel::markInitialized(uint64_t addr, size_t size) {
    MemoryRegion* region = findRegion(addr);
    if (region) {
        region->markInitialized(addr, size);
    }
}

CheckResult MemoryModel::checkAccess(uint64_t addr, size_t size,
                                     bool is_write, bool check_init) {
    // Check for null pointer
    if (isNullPointer(addr)) {
        return CheckResult(CheckResult::Status::NullPointerDeref, nullptr,
                          "Dereferencing null pointer");
    }
    
    // Find the region containing this address
    MemoryRegion* region = findRegion(addr);
    
    if (!region) {
        std::ostringstream oss;
        oss << "Access to invalid pointer 0x";
        oss << std::hex << addr << std::dec;
        oss << " (not in any allocated region)";
        return CheckResult(CheckResult::Status::InvalidPointer, nullptr, oss.str());
    }
    
    // Check if region is freed
    if (region->getState() == MemoryState::Freed) {
        std::ostringstream oss;
        oss << "Access to freed memory at 0x";
        oss << std::hex << addr << std::dec;
        oss << " (region base=0x";
        oss << std::hex << region->getBase() << std::dec << ")";
        return CheckResult(CheckResult::Status::UseAfterFree, region, oss.str());
    }
    
    // Check bounds
    if (!region->isValidAccess(addr, size)) {
        std::ostringstream oss;
        oss << "Out-of-bounds access: addr=0x";
        oss << std::hex << addr << std::dec;
        oss << ", size=" << size
            << ", region=[0x";
        oss << std::hex << region->getBase() << std::dec;
        oss << ", 0x";
        oss << std::hex << (region->getBase() + region->getSize()) << std::dec;
        oss << ")";
        return CheckResult(CheckResult::Status::OutOfBounds, region, oss.str());
    }
    
    // Check initialization (only for reads)
    if (!is_write && check_init) {
        if (!region->isInitialized(addr, size)) {
            std::ostringstream oss;
            oss << "Reading uninitialized memory at 0x";
            oss << std::hex << addr << std::dec;
            oss << " (size=" << size << ")";
            return CheckResult(CheckResult::Status::UninitializedRead, region, oss.str());
        }
    }
    
    // If this is a write, mark the bytes as initialized
    if (is_write) {
        region->markInitialized(addr, size);
    }
    
    return CheckResult(CheckResult::Status::OK, region, "");
}

CheckResult MemoryModel::checkPointerDeref(uint64_t ptr, size_t size,
                                           bool is_write, bool check_init) {
    return checkAccess(ptr, size, is_write, check_init);
}

MemoryRegion* MemoryModel::findRegion(uint64_t addr) {
    // Use efficient lookup via addr_to_base mapping
    auto it = addr_to_base_.upper_bound(addr);
    
    if (it != addr_to_base_.begin()) {
        --it;
        uint64_t base = it->second;
        auto region_it = regions_.find(base);
        if (region_it != regions_.end() && region_it->second.contains(addr)) {
            return &region_it->second;
        }
    }
    
    return nullptr;
}

const MemoryRegion* MemoryModel::findRegion(uint64_t addr) const {
    return const_cast<MemoryModel*>(this)->findRegion(addr);
}

MemoryRegion* MemoryModel::findRegionByBase(uint64_t base) {
    auto it = regions_.find(base);
    return (it != regions_.end()) ? &it->second : nullptr;
}

void MemoryModel::popStackFrame(const std::vector<uint64_t>& stack_addrs) {
    // Mark all stack allocations from this frame as freed
    // This enables detection of stack-use-after-return
    for (uint64_t base : stack_addrs) {
        auto* region = findRegionByBase(base);
        if (region && region->getState() == MemoryState::Allocated) {
            region->markFreed(nullptr);  // No specific free instruction for stack cleanup
        }
    }
}

std::vector<const MemoryRegion*> MemoryModel::getAllocatedRegions() const {
    std::vector<const MemoryRegion*> result;
    for (const auto& pair : regions_) {
        const MemoryRegion& region = pair.second;
        if (region.getState() == MemoryState::Allocated) {
            result.push_back(&region);
        }
    }
    return result;
}

std::vector<const MemoryRegion*> MemoryModel::getLeakedRegions() const {
    std::vector<const MemoryRegion*> result;
    for (const auto& pair : regions_) {
        const MemoryRegion& region = pair.second;
        if (region.getState() == MemoryState::Allocated &&
            region.getAllocType() == AllocationType::Heap) {
            result.push_back(&region);
        }
    }
    return result;
}

void MemoryModel::clear() {
    regions_.clear();
    addr_to_base_.clear();
    stack_frames_.clear();
    stack_depth_ = 0;
}

void MemoryModel::dump(llvm::raw_ostream& OS) const {
    OS << "Memory Model State:\n";
    OS << "  Total regions: " << regions_.size() << "\n";
    OS << "  Stack depth: " << stack_depth_ << "\n\n";
    
    for (const auto& pair : regions_) {
        OS << "  ";
        pair.second.dump(OS);
    }
}

size_t MemoryModel::getNumActiveAllocations() const {
    size_t count = 0;
    for (const auto& pair : regions_) {
        const MemoryRegion& region = pair.second;
        if (region.getState() == MemoryState::Allocated) {
            ++count;
        }
    }
    return count;
}

size_t MemoryModel::getTotalAllocatedBytes() const {
    size_t total = 0;
    for (const auto& pair : regions_) {
        const MemoryRegion& region = pair.second;
        if (region.getState() == MemoryState::Allocated) {
            total += region.getSize();
        }
    }
    return total;
}

void MemoryModel::updateAddressMapping(uint64_t base, size_t size) {
    // Add mapping for efficient lookup
    // We map every 256-byte boundary within the region to the base
    constexpr size_t granularity = 256;
    
    for (uint64_t addr = base; addr < base + size; addr += granularity) {
        addr_to_base_[addr] = base;
    }
    
    // Also map the last address
    if (size > 0) {
        addr_to_base_[base + size - 1] = base;
    }
}

void MemoryModel::removeAddressMapping(uint64_t base, size_t size) {
    constexpr size_t granularity = 256;
    
    for (uint64_t addr = base; addr < base + size; addr += granularity) {
        addr_to_base_.erase(addr);
    }
    
    if (size > 0) {
        addr_to_base_.erase(base + size - 1);
    }
}

MemoryRegion* MemoryModel::findRegionEfficient(uint64_t addr) {
    return findRegion(addr);
}

} // namespace miri

