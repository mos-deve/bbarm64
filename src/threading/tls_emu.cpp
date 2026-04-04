#include "tls_emu.hpp"
#include <sys/syscall.h>
#include <unistd.h>

namespace bbarm64 {

thread_local uint64_t TLSEmu::tls_base_ = 0;

TLSEmu::TLSEmu() {}

bool TLSEmu::set_tls_base(uint64_t addr) {
    tls_base_ = addr;
    // Set FS segment base via arch_prctl
    return syscall(SYS_arch_prctl, 0x1002, addr) == 0;  // ARCH_SET_FS
}

uint64_t TLSEmu::get_tls_base() const {
    return tls_base_;
}

bool TLSEmu::read_tls(uint64_t offset, void* dst, size_t size) {
    if (!tls_base_) return false;
    __builtin_memcpy(dst, reinterpret_cast<const void*>(tls_base_ + offset), size);
    return true;
}

bool TLSEmu::write_tls(uint64_t offset, const void* src, size_t size) {
    if (!tls_base_) return false;
    __builtin_memcpy(reinterpret_cast<void*>(tls_base_ + offset), src, size);
    return true;
}

} // namespace bbarm64
