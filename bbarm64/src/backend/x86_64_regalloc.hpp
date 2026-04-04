#pragma once
#include <cstdint>
#include <array>
#include <vector>

namespace bbarm64 {

// x86_64 physical registers
enum class X86Reg : uint8_t {
    RAX = 0, RCX = 1, RDX = 2, RBX = 3,
    RSP = 4, RBP = 5, RSI = 6, RDI = 7,
    R8 = 8, R9 = 9, R10 = 10, R11 = 11,
    R12 = 12, R13 = 13, R14 = 14, R15 = 15,
    NONE = 0xFF,
};

// Register allocator - maps ARM64 virtual regs to x86_64 physical regs
class X86_64RegAlloc {
public:
    X86_64RegAlloc();

    // Reset for a new block
    void reset();

    // Map an ARM64 register to an x86_64 register
    X86Reg map_arm_reg(uint8_t arm_reg);

    // Allocate a free x86_64 register for a virtual register
    X86Reg alloc_vreg(uint16_t vreg);

    // Get the x86_64 register for a virtual register
    X86Reg get_vreg(uint16_t vreg) const;

    // Spill a virtual register to memory
    void spill_vreg(uint16_t vreg, X86Reg xreg);

    // Reload a virtual register from memory
    void reload_vreg(uint16_t vreg);

    // Get all dirty vregs (need to be spilled)
    const std::vector<uint16_t>& dirty_vregs() const { return dirty_vregs_; }

    // ARM64 -> x86_64 static mapping for hot registers
    static X86Reg arm_to_x86_static(uint8_t arm_reg);

private:
    // Virtual register -> x86_64 register mapping
    std::array<X86Reg, 256> vreg_map_;
    // x86_64 register -> virtual register (reverse mapping)
    std::array<uint16_t, 16> xreg_map_;
    // Dirty virtual registers (modified, need spill)
    std::vector<uint16_t> dirty_vregs_;
    // Bitmask of free x86_64 registers
    uint16_t free_regs_;

    static constexpr uint16_t ALL_FREE = 0xFFFF;
};

} // namespace bbarm64
