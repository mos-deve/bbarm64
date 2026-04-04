#include "cache/translation_cache.hpp"

namespace bbarm64 {

// Patch block chaining slots for direct jumps
bool patch_chain_slot(BlockInfo* block, int slot, uint64_t target_addr) {
    if (!block || slot < 0 || slot > 1) return false;

    // The chain slot is at the end of the block's code
    // It's a JMP rel32 instruction (5 bytes: E9 xx xx xx xx)
    size_t chain_offset = block->host_code_size - 10;  // Two 5-byte JMP slots
    if (slot == 1) chain_offset += 5;

    uint8_t* slot_ptr = block->host_code + chain_offset;
    int32_t rel = static_cast<int32_t>(target_addr - (reinterpret_cast<uint64_t>(slot_ptr) + 5));

    slot_ptr[0] = 0xE9;  // JMP rel32
    *reinterpret_cast<int32_t*>(slot_ptr + 1) = rel;

    block->chain_slots[slot] = target_addr;
    return true;
}

// Try to chain to a target block
bool try_chain_block(TranslationCache& cache, BlockInfo* block, int slot, uint64_t target_pc) {
    BlockInfo* target = cache.lookup(target_pc);
    if (target && target->is_valid) {
        return patch_chain_slot(block, slot, reinterpret_cast<uint64_t>(target->host_code));
    }
    return false;
}

} // namespace bbarm64
