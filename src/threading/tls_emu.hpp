#pragma once
#include <cstdint>
#include <cstddef>

namespace bbarm64 {

// TLS emulation - maps ARM64 TPIDR_EL0 to x86_64 FS base
// Used by glibc and other dynamic binaries for thread-local storage
class TLSEmu {
public:
    TLSEmu();

    // Set TLS base for current thread (calls arch_prctl ARCH_SET_FS)
    bool set_tls_base(uint64_t addr);

    // Get TLS base for current thread
    uint64_t get_tls_base() const;

    // Read from TLS memory
    bool read_tls(uint64_t offset, void* dst, size_t size);

    // Write to TLS memory
    bool write_tls(uint64_t offset, const void* src, size_t size);

    // Allocate TLS block for a thread (returns guest address)
    static uint64_t allocate_tls_block(size_t size, class MemoryManager& mem);

private:
    thread_local static uint64_t tls_base_;
};

} // namespace bbarm64
