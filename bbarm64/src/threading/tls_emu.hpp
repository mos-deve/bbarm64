#pragma once
#include <cstdint>
#include <cstddef>
#include <cstddef>

namespace bbarm64 {

// TLS emulation - maps ARM64 TPIDR_EL0 to x86_64 FS base
class TLSEmu {
public:
    TLSEmu();

    // Set TLS base for current thread
    bool set_tls_base(uint64_t addr);

    // Get TLS base for current thread
    uint64_t get_tls_base() const;

    // Read from TLS memory
    bool read_tls(uint64_t offset, void* dst, size_t size);

    // Write to TLS memory
    bool write_tls(uint64_t offset, const void* src, size_t size);

private:
    thread_local static uint64_t tls_base_;
};

} // namespace bbarm64
