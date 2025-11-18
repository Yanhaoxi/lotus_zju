/**
 * @file MemoryModel.h
 * @brief Enhanced memory model for tracking allocations and detecting memory bugs

 */

#ifndef ANALYSIS_LLVMIR_EMUL_MEMORYMODEL_H
#define ANALYSIS_LLVMIR_EMUL_MEMORYMODEL_H

#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/raw_ostream.h>
#include <map>
#include <set>
#include <vector>
#include <string>
#include <cstdint>

namespace miri {

/**
 * State of a memory region
 */
enum class MemoryState {
    Unallocated,  // Not yet allocated
    Allocated,    // Currently allocated and valid
    Freed         // Has been freed
};

/**
 * Type of allocation
 */
enum class AllocationType {
    Stack,        // alloca instruction
    Heap,         // malloc/calloc/realloc
    Global,       // Global variable
    Unknown       // Unknown source
};

/**
 * Metadata for a memory region
 * Tracks allocation, state, and byte-level initialization
 */
class MemoryRegion {
public:
    MemoryRegion(uint64_t base, size_t sz, llvm::Instruction* site,
                 AllocationType type = AllocationType::Unknown)
        : base_address(base), size(sz), state(MemoryState::Allocated),
          alloc_site(site), free_site(nullptr), alloc_type(type),
          initialized_bytes(sz, false) {}
    
    // Accessors
    uint64_t getBase() const { return base_address; }
    size_t getSize() const { return size; }
    MemoryState getState() const { return state; }
    AllocationType getAllocType() const { return alloc_type; }
    llvm::Instruction* getAllocSite() const { return alloc_site; }
    llvm::Instruction* getFreeSite() const { return free_site; }
    
    /**
     * Check if address is within this region
     */
    bool contains(uint64_t addr) const {
        return addr >= base_address && addr < (base_address + size);
    }
    
    /**
     * Check if an access [addr, addr+access_size) is valid
     * Returns true if completely within bounds
     */
    bool isValidAccess(uint64_t addr, size_t access_size) const {
        if (addr < base_address) return false;
        uint64_t end_addr = addr + access_size;
        uint64_t region_end = base_address + size;
        return end_addr <= region_end;
    }
    
    /**
     * Check if bytes in range [addr, addr+len) are initialized
     */
    bool isInitialized(uint64_t addr, size_t len) const;
    
    /**
     * Mark bytes in range [addr, addr+len) as initialized
     */
    void markInitialized(uint64_t addr, size_t len);
    
    /**
     * Mark entire region as initialized (for calloc or explicit initialization)
     */
    void markFullyInitialized() {
        std::fill(initialized_bytes.begin(), initialized_bytes.end(), true);
    }
    
    /**
     * Mark this region as freed
     */
    void markFreed(llvm::Instruction* free_instr) {
        state = MemoryState::Freed;
        free_site = free_instr;
    }
    
    /**
     * Check if region is currently valid for access
     */
    bool isValid() const {
        return state == MemoryState::Allocated;
    }
    
    /**
     * Print region information for debugging
     */
    void dump(llvm::raw_ostream& OS) const;
    
private:
    uint64_t base_address;
    size_t size;
    MemoryState state;
    llvm::Instruction* alloc_site;
    llvm::Instruction* free_site;
    AllocationType alloc_type;
    
    // Byte-level initialization tracking
    std::vector<bool> initialized_bytes;
};

/**
 * Result of a memory access check
 */
struct CheckResult {
    enum class Status {
        OK,                  // Access is valid
        OutOfBounds,         // Access outside allocated region
        UseAfterFree,        // Access to freed memory
        UninitializedRead,   // Reading uninitialized memory
        NullPointerDeref,    // Dereferencing null pointer
        InvalidPointer,      // Pointer not in any known region
        DoubleFree          // Attempting to free already-freed memory
    };
    
    Status status;
    MemoryRegion* region;  // The affected region (if found)
    std::string message;
    
    CheckResult() : status(Status::OK), region(nullptr) {}
    
    CheckResult(Status s, MemoryRegion* r = nullptr, const std::string& msg = "")
        : status(s), region(r), message(msg) {}
    
    bool isOK() const { return status == Status::OK; }
    
    const char* getStatusString() const;
};

/**
 * Enhanced memory model with allocation tracking and safety checks
 */
class MemoryModel {
public:
    MemoryModel() = default;
    ~MemoryModel() = default;
    
    /**
     * Register a new allocation
     * Returns true if successful, false if overlaps with existing allocation
     */
    bool registerAllocation(uint64_t base, size_t size,
                           llvm::Instruction* alloc_site,
                           AllocationType type = AllocationType::Unknown);
    
    /**
     * Register a stack allocation (from alloca)
     * Automatically tracks for cleanup when stack frame pops
     */
    bool registerStackAllocation(uint64_t base, size_t size,
                                llvm::AllocaInst* alloc_site);
    
    /**
     * Register a heap allocation (from malloc/calloc)
     */
    bool registerHeapAllocation(uint64_t base, size_t size,
                               llvm::Instruction* alloc_site,
                               bool zero_initialized = false);
    
    /**
     * Register a global variable
     */
    bool registerGlobalVariable(uint64_t base, size_t size,
                               llvm::GlobalVariable* gv);
    
    /**
     * Mark memory as freed
     * Returns CheckResult indicating if free is valid
     */
    CheckResult markFreed(uint64_t addr, llvm::Instruction* free_site);
    
    /**
     * Mark bytes as initialized
     */
    void markInitialized(uint64_t addr, size_t size);
    
    /**
     * Check if a memory access is valid
     * @param addr Starting address
     * @param size Number of bytes to access
     * @param is_write Whether this is a write (vs read)
     * @param check_init Whether to check initialization (only for reads)
     */
    CheckResult checkAccess(uint64_t addr, size_t size,
                           bool is_write, bool check_init = true);
    
    /**
     * Check if a pointer dereference is valid
     * Convenience wrapper for checkAccess
     */
    CheckResult checkPointerDeref(uint64_t ptr, size_t size,
                                 bool is_write, bool check_init = true);
    
    /**
     * Find the memory region containing an address
     * Returns nullptr if not found
     */
    MemoryRegion* findRegion(uint64_t addr);
    const MemoryRegion* findRegion(uint64_t addr) const;
    
    /**
     * Find region by base address
     */
    MemoryRegion* findRegionByBase(uint64_t base);
    
    /**
     * Check if address is a null pointer (0 or within small threshold)
     */
    bool isNullPointer(uint64_t addr) const {
        return addr < null_pointer_threshold_;
    }
    
    /**
     * Handle stack frame cleanup
     * Marks all addresses in the list as freed (use-after-return detection)
     */
    void popStackFrame(const std::vector<uint64_t>& stack_addrs);
    
    /**
     * Track current stack depth for stack-use-after-return detection
     */
    void pushStackFrame() { ++stack_depth_; }
    void popStackFrameMarker() { --stack_depth_; }
    
    /**
     * Get all allocated regions (for leak detection)
     */
    std::vector<const MemoryRegion*> getAllocatedRegions() const;
    
    /**
     * Get all heap regions that haven't been freed (for leak detection)
     */
    std::vector<const MemoryRegion*> getLeakedRegions() const;
    
    /**
     * Clear all memory state (for testing)
     */
    void clear();
    
    /**
     * Print memory state for debugging
     */
    void dump(llvm::raw_ostream& OS) const;
    
    /**
     * Statistics
     */
    size_t getNumAllocations() const { return regions_.size(); }
    size_t getNumActiveAllocations() const;
    size_t getTotalAllocatedBytes() const;
    
private:
    // Map from base address to memory region
    std::map<uint64_t, MemoryRegion> regions_;
    
    // Quick lookup: maps any address to its region's base
    // For efficient lookup of which region contains an address
    std::map<uint64_t, uint64_t> addr_to_base_;
    
    // Stack allocations per frame (for cleanup)
    std::vector<std::set<uint64_t>> stack_frames_;
    unsigned stack_depth_ = 0;
    
    // Null pointer threshold (addresses below this are considered null)
    static constexpr uint64_t null_pointer_threshold_ = 4096;
    
    /**
     * Update address-to-base mapping for a region
     */
    void updateAddressMapping(uint64_t base, size_t size);
    
    /**
     * Remove address-to-base mapping for a region
     */
    void removeAddressMapping(uint64_t base, size_t size);
    
    /**
     * Find region containing address using efficient lookup
     */
    MemoryRegion* findRegionEfficient(uint64_t addr);
};

} // namespace miri

#endif // ANALYSIS_LLVMIR_EMUL_MEMORYMODEL_H

