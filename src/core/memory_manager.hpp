#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <mutex>

namespace bbarm64 {

// Memory page size (4KB, matching ARM64 Linux default)
static constexpr size_t PAGE_SIZE = 4096;
static constexpr size_t PAGE_MASK = ~(PAGE_SIZE - 1);

// Memory region types
enum class MemRegionType {
    CODE,       // Executable code (from ELF)
    DATA,       // Read/write data
    STACK,      // Stack memory
    HEAP,       // Heap (brk/mmap)
    MMAP,       // Memory-mapped files
    GUARD,      // Guard page (unmapped)
};

struct MemRegion {
    uint64_t addr;
    size_t size;
    uint32_t perms;  // PROT_READ | PROT_WRITE | PROT_EXEC
    MemRegionType type;
    bool is_translated;  // Has code been translated from this region?
};

class MemoryManager {
public:
    MemoryManager();
    ~MemoryManager();

    // Initialize memory space
    bool init(size_t total_size = 1ULL << 32);  // 4GB default

    // Map/unmap memory
    uint64_t mmap(size_t size, uint32_t prot, uint64_t hint_addr = 0);
    int munmap(uint64_t addr, size_t size);
    int mprotect(uint64_t addr, size_t size, uint32_t prot);

    // Memory access (safe, with bounds checking)
    bool read_u8(uint64_t addr, uint8_t& out);
    bool read_u16(uint64_t addr, uint16_t& out);
    bool read_u32(uint64_t addr, uint32_t& out);
    bool read_u64(uint64_t addr, uint64_t& out);
    bool read_bytes(uint64_t addr, void* dst, size_t len);

    bool write_u8(uint64_t addr, uint8_t val);
    bool write_u16(uint64_t addr, uint16_t val);
    bool write_u32(uint64_t addr, uint32_t val);
    bool write_u64(uint64_t addr, uint64_t val);
    bool write_bytes(uint64_t addr, const void* src, size_t len);

    // Direct pointer access (for performance-critical paths)
    void* get_host_ptr(uint64_t addr);
    const void* get_host_ptr(uint64_t addr) const;

    // SMC detection
    bool is_translated_page(uint64_t addr) const;
    void mark_translated_page(uint64_t addr);
    void invalidate_translated_pages();

    // Memory map
    const std::vector<MemRegion>& regions() const { return regions_; }

    // Brk (heap pointer)
    uint64_t get_brk() const { return brk_; }
    int set_brk(uint64_t new_brk);

private:
    uint8_t* memory_base_;
    size_t memory_size_;
    uint64_t brk_;
    uint64_t mmap_base_;
    std::vector<MemRegion> regions_;
    mutable std::mutex mutex_;

    // Track which pages have been translated (for SMC detection)
    std::vector<uint64_t> translated_pages_;  // Page addresses

    uint64_t find_free_region(size_t size, uint64_t hint);
    bool check_perms(uint64_t addr, size_t len, uint32_t needed) const;
};

} // namespace bbarm64
