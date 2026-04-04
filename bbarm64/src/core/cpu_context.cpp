#include "cpu_context.hpp"

namespace bbarm64 {

// CPU context implementation - mostly inline in header
// This file provides utility functions

void cpu_context_reset(CPUContext& ctx) {
    ctx = CPUContext{};
}

void cpu_context_set_regs(CPUContext& ctx, const uint64_t* regs, int count) {
    for (int i = 0; i < count && i < 31; i++) {
        ctx.x[i] = regs[i];
    }
}

uint64_t cpu_context_get_reg(const CPUContext& ctx, int reg) {
    if (reg >= 0 && reg < 31) return ctx.x[reg];
    if (reg == 31) return ctx.sp;
    return 0;
}

void cpu_context_set_reg(CPUContext& ctx, int reg, uint64_t value) {
    if (reg >= 0 && reg < 31) ctx.x[reg] = value;
    else if (reg == 31) ctx.sp = value;
}

} // namespace bbarm64
