#include "x86_64_regalloc.hpp"
#include <cstring>

namespace bbarm64 {

X86_64RegAlloc::X86_64RegAlloc() : free_regs_(ALL_FREE) {
    vreg_map_.fill(X86Reg::NONE);
    xreg_map_.fill(0);
}

void X86_64RegAlloc::reset() {
    vreg_map_.fill(X86Reg::NONE);
    xreg_map_.fill(0);
    dirty_vregs_.clear();
    free_regs_ = ALL_FREE;
}

X86Reg X86_64RegAlloc::arm_to_x86_static(uint8_t arm_reg) {
    static const X86Reg mapping[] = {
        X86Reg::RAX,  // X0
        X86Reg::RCX,  // X1
        X86Reg::RDX,  // X2
        X86Reg::RBX,  // X3
        X86Reg::RSI,  // X4
        X86Reg::RDI,  // X5
        X86Reg::R8,   // X6
        X86Reg::R9,   // X7
        X86Reg::R10,  // X8
        X86Reg::R11,  // X9
        X86Reg::R13,  // X10
        X86Reg::R14,  // X11
        X86Reg::R15,  // X12
        X86Reg::RSI,  // X13
        X86Reg::RDI,  // X14
        X86Reg::R8,   // X15
    };
    if (arm_reg < 16) return mapping[arm_reg];
    if (arm_reg == 29) return X86Reg::R9;
    if (arm_reg == 30) return X86Reg::R11;
    if (arm_reg == 31) return X86Reg::RAX;
    return X86Reg::NONE;
}

X86Reg X86_64RegAlloc::map_arm_reg(uint8_t arm_reg) {
    return arm_to_x86_static(arm_reg);
}

X86Reg X86_64RegAlloc::alloc_vreg(uint16_t vreg) {
    if (vreg < 256 && vreg_map_[vreg] != X86Reg::NONE) {
        return vreg_map_[vreg];
    }

    // Find a free register
    for (int i = 0; i < 16; i++) {
        if (free_regs_ & (1 << i)) {
            X86Reg reg = static_cast<X86Reg>(i);
            free_regs_ &= ~(1 << i);
            vreg_map_[vreg] = reg;
            xreg_map_[i] = vreg;
            return reg;
        }
    }

    // No free registers - spill the least recently used
    // For now, just use RAX as a scratch
    return X86Reg::RAX;
}

X86Reg X86_64RegAlloc::get_vreg(uint16_t vreg) const {
    if (vreg < 256) return vreg_map_[vreg];
    return X86Reg::NONE;
}

void X86_64RegAlloc::spill_vreg(uint16_t vreg, X86Reg xreg) {
    (void)vreg;
    (void)xreg;
    // In a full implementation, emit store to spill slot
}

void X86_64RegAlloc::reload_vreg(uint16_t vreg) {
    (void)vreg;
    // In a full implementation, emit load from spill slot
}

} // namespace bbarm64
