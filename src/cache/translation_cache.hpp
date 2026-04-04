#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <unordered_map>

namespace bbarm64 {

// Translated block metadata
struct BlockInfo {
    uint64_t arm_pc;           // Original ARM64 program counter
    uint8_t* host_code;        // Pointer to translated x86_64 code
    uint32_t host_code_size;   // Size of translated code in bytes
    uint32_t arm_instr_count;  // Number of ARM64 instructions in block
    uint64_t chain_slots[2];   // Direct chain targets (0=fallthrough, 1=branch)
    uint32_t hit_count;        // For profiling
    bool is_valid;             // False if invalidated (SMC)

    BlockInfo()
        : arm_pc(0), host_code(nullptr), host_code_size(0),
          arm_instr_count(0), hit_count(0), is_valid(true) {
        chain_slots[0] = 0;
        chain_slots[1] = 0;
    }
};

// L1 In-memory translation cache
class TranslationCache {
public:
    TranslationCache(size_t initial_capacity = 65536);
    ~TranslationCache();

    // Initialize the cache
    bool init();

    // Lookup a translated block by ARM64 PC
    BlockInfo* lookup(uint64_t arm_pc);

    // Insert a new translated block
    bool insert(uint64_t arm_pc, uint8_t* host_code, uint32_t host_code_size,
                uint32_t arm_instr_count);

    // Invalidate a block (for SMC)
    void invalidate(uint64_t arm_pc);

    // Invalidate all blocks in a page range
    void invalidate_range(uint64_t start, uint64_t end);

    // Get code pool for allocation
    uint8_t* alloc_host_code(size_t size);

    // Stats
    size_t size() const { return block_map_.size(); }
    size_t capacity() const { return capacity_; }
    size_t code_pool_used() const { return code_pool_used_; }

private:
    std::unordered_map<uint64_t, BlockInfo> block_map_;
    size_t capacity_;

    // Executable memory pool
    uint8_t* code_pool_;
    size_t code_pool_size_;
    size_t code_pool_used_;

    static constexpr size_t DEFAULT_CODE_POOL_SIZE = 64 * 1024 * 1024; // 64MB
};

} // namespace bbarm64
