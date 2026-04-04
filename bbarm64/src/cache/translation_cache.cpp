#include "translation_cache.hpp"
#include <sys/mman.h>
#include <cstring>
#include <iostream>

namespace bbarm64 {

TranslationCache::TranslationCache(size_t initial_capacity)
    : capacity_(initial_capacity), code_pool_(nullptr),
      code_pool_size_(DEFAULT_CODE_POOL_SIZE), code_pool_used_(0) {}

TranslationCache::~TranslationCache() {
    if (code_pool_) {
        munmap(code_pool_, code_pool_size_);
    }
}

bool TranslationCache::init() {
    code_pool_ = static_cast<uint8_t*>(
        mmap(nullptr, code_pool_size_, PROT_READ | PROT_WRITE | PROT_EXEC,
             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)
    );
    if (code_pool_ == MAP_FAILED) {
        code_pool_ = nullptr;
        return false;
    }
    block_map_.reserve(capacity_);
    return true;
}

BlockInfo* TranslationCache::lookup(uint64_t arm_pc) {
    auto it = block_map_.find(arm_pc);
    if (it != block_map_.end() && it->second.is_valid) {
        it->second.hit_count++;
        return &it->second;
    }
    return nullptr;
}

bool TranslationCache::insert(uint64_t arm_pc, uint8_t* host_code,
                               uint32_t host_code_size, uint32_t arm_instr_count) {
    BlockInfo info;
    info.arm_pc = arm_pc;
    info.host_code = host_code;
    info.host_code_size = host_code_size;
    info.arm_instr_count = arm_instr_count;
    info.is_valid = true;
    
    // Critical: Flush instruction cache for newly generated code
    __builtin___clear_cache(reinterpret_cast<char*>(host_code), 
                            reinterpret_cast<char*>(host_code + host_code_size));
    
    block_map_[arm_pc] = info;
    return true;
}

void TranslationCache::invalidate(uint64_t arm_pc) {
    auto it = block_map_.find(arm_pc);
    if (it != block_map_.end()) {
        it->second.is_valid = false;
    }
}

void TranslationCache::invalidate_range(uint64_t start, uint64_t end) {
    for (auto& [pc, info] : block_map_) {
        if (pc >= start && pc < end) {
            info.is_valid = false;
        }
    }
}

uint8_t* TranslationCache::alloc_host_code(size_t size) {
    if (code_pool_used_ + size > code_pool_size_) {
        // Pool exhausted - in production, grow or allocate new pool
        return nullptr;
    }
    uint8_t* ptr = code_pool_ + code_pool_used_;
    code_pool_used_ += size;
    return ptr;
}

} // namespace bbarm64
