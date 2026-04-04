#include "tls_emu.hpp"
#include "../core/memory_manager.hpp"
#include <sys/syscall.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>

namespace bbarm64 {

thread_local uint64_t TLSEmu::tls_base_ = 0;

TLSEmu::TLSEmu() {}

bool TLSEmu::set_tls_base(uint64_t addr) {
    tls_base_ = addr;
    // Set FS segment base via arch_prctl (ARCH_SET_FS = 0x1002)
    long ret = syscall(SYS_arch_prctl, 0x1002, addr);
    return ret == 0;
}

uint64_t TLSEmu::get_tls_base() const {
    return tls_base_;
}

bool TLSEmu::read_tls(uint64_t offset, void* dst, size_t size) {
    if (!tls_base_) return false;
    std::memcpy(dst, reinterpret_cast<const void*>(tls_base_ + offset), size);
    return true;
}

bool TLSEmu::write_tls(uint64_t offset, const void* src, size_t size) {
    if (!tls_base_) return false;
    std::memcpy(reinterpret_cast<void*>(tls_base_ + offset), src, size);
    return true;
}

// Allocate a TLS block in guest memory with proper alignment
// ARM64 TLS blocks are typically 64-byte aligned
uint64_t TLSEmu::allocate_tls_block(size_t size, MemoryManager& mem) {
    // Round up to 64-byte alignment
    size = (size + 63) & ~63ULL;
    // Add space for the TCB header (at least 16 bytes for the thread pointer itself)
    size += 64;
    uint64_t addr = mem.mmap(size, PROT_READ | PROT_WRITE, 0);
    if (!addr) return 0;
    // Zero the block
    uint8_t* ptr = reinterpret_cast<uint8_t*>(addr);
    std::memset(ptr, 0, size);
    return addr;
}

} // namespace bbarm64
