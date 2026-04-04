#pragma once
#include <cstdint>
#include <array>

namespace bbarm64 {

// ARM64 CPU context - all 31 GPRs + SP, PC, NZCV flags
struct alignas(16) CPUContext {
    // General purpose registers X0-X30
    std::array<uint64_t, 31> x;
    uint64_t sp;       // Stack pointer
    uint64_t pc;       // Program counter
    uint64_t lr;       // Link register (alias of X30, kept for clarity)

    // NZCV flags packed into 64-bit value
    // Bits: [31]=N, [30]=Z, [29]=C, [28]=V
    uint64_t nzcv;

    // SIMD/FP registers V0-V31 (128-bit each)
    alignas(16) uint8_t v[32][16];

    // FPCR/FPSR
    uint32_t fpcr;
    uint32_t fpsr;

    // TLS register (TPIDR_EL0 emulation)
    uint64_t tpidr_el0;

    // Thread ID (for threading)
    uint64_t tid;

    // CPU state flags
    uint32_t state_flags;
    static constexpr uint32_t STATE_SINGLE_STEP = (1u << 0);
    static constexpr uint32_t STATE_IN_SYSCALL = (1u << 1);
    static constexpr uint32_t STATE_CRASHED = (1u << 2);
    static constexpr uint32_t STATE_EXITED = (1u << 3);

    // Exit status (set when STATE_EXITED is set)
    int exit_status;

    CPUContext() {
        x.fill(0);
        sp = 0;
        pc = 0;
        lr = 0;
        nzcv = 0;
        for (auto& reg : v) for (int i = 0; i < 16; i++) reg[i] = 0;
        fpcr = 0;
        fpsr = 0;
        tpidr_el0 = 0;
        tid = 0;
        state_flags = 0;
        exit_status = 0;
    }

    // NZCV accessors
    bool flag_n() const { return (nzcv >> 31) & 1; }
    bool flag_z() const { return (nzcv >> 30) & 1; }
    bool flag_c() const { return (nzcv >> 29) & 1; }
    bool flag_v() const { return (nzcv >> 28) & 1; }

    void set_n(bool v) { nzcv = (nzcv & ~(1ULL << 31)) | ((uint64_t)v << 31); }
    void set_z(bool v) { nzcv = (nzcv & ~(1ULL << 30)) | ((uint64_t)v << 30); }
    void set_c(bool v) { nzcv = (nzcv & ~(1ULL << 29)) | ((uint64_t)v << 29); }
    void set_v(bool v) { nzcv = (nzcv & ~(1ULL << 28)) | ((uint64_t)v << 28); }

    void set_nzcv(bool n, bool z, bool c, bool v) {
        nzcv = ((uint64_t)n << 31) | ((uint64_t)z << 30) |
               ((uint64_t)c << 29) | ((uint64_t)v << 28);
    }
};

} // namespace bbarm64
