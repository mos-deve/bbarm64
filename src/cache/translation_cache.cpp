#include "translation_cache.hpp"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <algorithm>

namespace bbarm64 {

TranslationCache::TranslationCache(size_t initial_capacity)
    : capacity_(initial_capacity), code_pool_(nullptr),
      code_pool_size_(DEFAULT_CODE_POOL_SIZE), code_pool_used_(0),
      access_counter_(0) {}

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
        it->second.last_access = ++access_counter_;
        // Auto-promote hot blocks
        if (it->second.hit_count >= HOT_THRESHOLD) {
            it->second.is_hot = true;
        }
        return &it->second;
    }
    return nullptr;
}

bool TranslationCache::insert(uint64_t arm_pc, uint8_t* host_code,
                               uint32_t host_code_size, uint32_t arm_instr_count) {
    // Evict cold blocks if cache is getting full
    if (block_map_.size() >= capacity_) {
        evict_cold();
    }

    BlockInfo info;
    info.arm_pc = arm_pc;
    info.host_code = host_code;
    info.host_code_size = host_code_size;
    info.arm_instr_count = arm_instr_count;
    info.is_valid = true;
    info.last_access = ++access_counter_;
    info.hit_count = 0;

    // Flush instruction cache for newly generated code
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
        // Try to evict cold blocks and compact
        evict_cold();
        if (code_pool_used_ + size > code_pool_size_) {
            return nullptr;
        }
    }
    uint8_t* ptr = code_pool_ + code_pool_used_;
    code_pool_used_ += size;
    return ptr;
}

void TranslationCache::promote_hot(uint64_t arm_pc) {
    auto it = block_map_.find(arm_pc);
    if (it != block_map_.end()) {
        it->second.is_hot = true;
        it->second.hit_count = HOT_THRESHOLD;
    }
}

void TranslationCache::evict_cold() {
    // Find and remove the least recently accessed non-hot block
    uint64_t min_access = UINT64_MAX;
    uint64_t evict_pc = 0;
    bool found = false;

    for (const auto& [pc, info] : block_map_) {
        if (!info.is_hot && info.is_valid && info.last_access < min_access) {
            min_access = info.last_access;
            evict_pc = pc;
            found = true;
        }
    }

    if (found) {
        block_map_.erase(evict_pc);
    }
}

size_t TranslationCache::hot_block_count() const {
    size_t count = 0;
    for (const auto& [pc, info] : block_map_) {
        if (info.is_hot) count++;
    }
    return count;
}

// AOT: Save translated block to disk cache
// Format: [magic(4)] [arm_pc(8)] [host_size(4)] [arm_instrs(4)] [code_bytes...]
bool TranslationCache::save_aot_block(uint64_t arm_pc, const uint8_t* host_code,
                                       uint32_t host_code_size, uint32_t arm_instr_count,
                                       const std::string& aot_dir) {
    // Create directory if needed
    mkdir(aot_dir.c_str(), 0755);

    char path[512];
    snprintf(path, sizeof(path), "%s/%016lx.fae", aot_dir.c_str(), arm_pc);

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return false;

    // Write header
    uint32_t magic = 0x46414543; // "FAEC"
    write(fd, &magic, 4);
    write(fd, &arm_pc, 8);
    write(fd, &host_code_size, 4);
    write(fd, &arm_instr_count, 4);

    // Write code
    write(fd, host_code, host_code_size);

    close(fd);
    return true;
}

// AOT: Load translated block from disk cache
uint8_t* TranslationCache::load_aot_block(uint64_t arm_pc, uint32_t& out_size,
                                           uint32_t& out_instr_count, const std::string& aot_dir) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%016lx.fae", aot_dir.c_str(), arm_pc);

    int fd = open(path, O_RDONLY);
    if (fd < 0) return nullptr;

    // Read and verify header
    uint32_t magic;
    if (read(fd, &magic, 4) != 4 || magic != 0x46414543) {
        close(fd);
        return nullptr;
    }

    uint64_t stored_pc;
    uint32_t host_size;
    read(fd, &stored_pc, 8);
    read(fd, &host_size, 4);
    read(fd, &out_instr_count, 4);

    if (stored_pc != arm_pc) {
        close(fd);
        return nullptr;
    }

    // Allocate executable memory
    uint8_t* code = static_cast<uint8_t*>(
        mmap(nullptr, host_size, PROT_READ | PROT_WRITE | PROT_EXEC,
             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)
    );
    if (code == MAP_FAILED) {
        close(fd);
        return nullptr;
    }

    // Read code
    ssize_t n = read(fd, code, host_size);
    close(fd);

    if (n != static_cast<ssize_t>(host_size)) {
        munmap(code, host_size);
        return nullptr;
    }

    out_size = host_size;
    return code;
}

} // namespace bbarm64
