#include "memory_manager.hpp"
#include <sys/mman.h>
#include <cstring>
#include <algorithm>

namespace bbarm64 {

MemoryManager::MemoryManager()
    : memory_base_(nullptr), memory_size_(0), brk_(0), mmap_base_(0) {}

MemoryManager::~MemoryManager() {
    if (memory_base_) {
        ::munmap(memory_base_, memory_size_);
    }
}

bool MemoryManager::init(size_t total_size) {
    std::lock_guard<std::mutex> lock(mutex_);
    memory_size_ = total_size;
    memory_base_ = static_cast<uint8_t*>(
        ::mmap(nullptr, total_size, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)
    );
    if (memory_base_ == MAP_FAILED) {
        memory_base_ = nullptr;
        return false;
    }
    mmap_base_ = reinterpret_cast<uint64_t>(memory_base_) + (total_size / 2);
    brk_ = mmap_base_;
    return true;
}

uint64_t MemoryManager::mmap(size_t size, uint32_t prot, uint64_t hint_addr) {
    std::lock_guard<std::mutex> lock(mutex_);
    size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);  // Page align

    uint64_t addr = hint_addr ? hint_addr : find_free_region(size, 0);
    if (!addr) return 0;

    // Map in host memory
    void* host_ptr = ::mmap(reinterpret_cast<void*>(addr), size,
                            prot, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (host_ptr == MAP_FAILED) return 0;

    regions_.push_back({addr, size, prot, MemRegionType::MMAP, false});
    return addr;
}

int MemoryManager::munmap(uint64_t addr, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    // Check for translated pages and invalidate
    for (auto it = regions_.begin(); it != regions_.end(); ) {
        if (it->addr >= addr && it->addr < addr + size && it->is_translated) {
            invalidate_translated_pages();
        }
        if (it->addr >= addr && it->addr < addr + size) {
            it = regions_.erase(it);
        } else {
            ++it;
        }
    }

    return ::munmap(reinterpret_cast<void*>(addr), size);
}

int MemoryManager::mprotect(uint64_t addr, size_t size, uint32_t prot) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& region : regions_) {
        if (region.addr == addr) {
            region.perms = prot;
            return ::mprotect(reinterpret_cast<void*>(addr), size, prot);
        }
    }
    return ::mprotect(reinterpret_cast<void*>(addr), size, prot);
}

bool MemoryManager::read_u8(uint64_t addr, uint8_t& out) {
    if (!check_perms(addr, 1, PROT_READ)) return false;
    out = *reinterpret_cast<const uint8_t*>(addr);
    return true;
}

bool MemoryManager::read_u16(uint64_t addr, uint16_t& out) {
    if (!check_perms(addr, 2, PROT_READ)) return false;
    out = *reinterpret_cast<const uint16_t*>(addr);
    return true;
}

bool MemoryManager::read_u32(uint64_t addr, uint32_t& out) {
    // Read directly - guest virtual address maps 1:1 to host virtual address
    // We already mapped it at exactly this address during ELF load
    out = *reinterpret_cast<const uint32_t*>(addr);
    return true;
}

bool MemoryManager::read_u64(uint64_t addr, uint64_t& out) {
    if (!check_perms(addr, 8, PROT_READ)) return false;
    out = *reinterpret_cast<const uint64_t*>(addr);
    return true;
}

bool MemoryManager::read_bytes(uint64_t addr, void* dst, size_t len) {
    if (!check_perms(addr, len, PROT_READ)) return false;
    std::memcpy(dst, reinterpret_cast<const void*>(addr), len);
    return true;
}

bool MemoryManager::write_u8(uint64_t addr, uint8_t val) {
    if (!check_perms(addr, 1, PROT_WRITE)) return false;
    *reinterpret_cast<uint8_t*>(addr) = val;
    return true;
}

bool MemoryManager::write_u16(uint64_t addr, uint16_t val) {
    if (!check_perms(addr, 2, PROT_WRITE)) return false;
    *reinterpret_cast<uint16_t*>(addr) = val;
    return true;
}

bool MemoryManager::write_u32(uint64_t addr, uint32_t val) {
    if (!check_perms(addr, 4, PROT_WRITE)) return false;
    *reinterpret_cast<uint32_t*>(addr) = val;
    return true;
}

bool MemoryManager::write_u64(uint64_t addr, uint64_t val) {
    if (!check_perms(addr, 8, PROT_WRITE)) return false;
    *reinterpret_cast<uint64_t*>(addr) = val;
    return true;
}

bool MemoryManager::write_bytes(uint64_t addr, const void* src, size_t len) {
    if (!check_perms(addr, len, PROT_WRITE)) return false;
    std::memcpy(reinterpret_cast<void*>(addr), src, len);
    return true;
}

void* MemoryManager::get_host_ptr(uint64_t addr) {
    return reinterpret_cast<void*>(addr);
}

const void* MemoryManager::get_host_ptr(uint64_t addr) const {
    return reinterpret_cast<const void*>(addr);
}

bool MemoryManager::is_translated_page(uint64_t addr) const {
    uint64_t page = addr & PAGE_MASK;
    for (auto p : translated_pages_) {
        if (p == page) return true;
    }
    return false;
}

void MemoryManager::mark_translated_page(uint64_t addr) {
    uint64_t page = addr & PAGE_MASK;
    for (auto p : translated_pages_) {
        if (p == page) return;
    }
    translated_pages_.push_back(page);
}

void MemoryManager::invalidate_translated_pages() {
    translated_pages_.clear();
}

int MemoryManager::set_brk(uint64_t new_brk) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (new_brk < mmap_base_) return -1;
    brk_ = new_brk;
    return 0;
}

uint64_t MemoryManager::find_free_region(size_t size, uint64_t hint) {
    if (hint) {
        // Always use hint address - this is critical for ELF segment loading at fixed addresses
        return hint;
    }
    uint64_t addr = mmap_base_;
    mmap_base_ += size;
    return addr;
}

bool MemoryManager::check_perms(uint64_t addr, size_t len, uint32_t needed) const {
    for (const auto& region : regions_) {
        if (addr >= region.addr && addr + len <= region.addr + region.size) {
            // Special case: EXEC pages are always readable for instruction fetch
            if ((needed == PROT_READ) && (region.perms & PROT_EXEC)) {
                return true;
            }
            return (region.perms & needed) == needed;
        }
    }
    return false;
}

} // namespace bbarm64
