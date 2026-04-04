#include "x86_64_emitter.hpp"
#include "x86_64_regalloc.hpp"
#include "../ir/ir.hpp"
#include "../core/cpu_context.hpp"

namespace bbarm64 {

static constexpr size_t CTX_NZCV_OFFSET = 272;
static constexpr size_t CTX_PC_OFFSET = 256;

class IRLowering {
public:
    IRLowering(X86_64Emitter& emitter, X86_64RegAlloc& regalloc);

    bool lower_block(const IRBlock& block, CPUContext& ctx, uint64_t fallthrough_pc);
    bool lower_instr(const IRInst& instr);

private:
    X86_64Emitter& emitter_;
    X86_64RegAlloc& regalloc_;
    uint64_t fallthrough_pc_;

    uint8_t arm_reg_to_x86(uint8_t arm_reg);
    void load_arm_reg(uint8_t arm_reg, uint8_t xreg);
    void store_arm_reg(uint8_t xreg, uint8_t arm_reg);
    void save_nzcv();
    void emit_block_ret(uint64_t target_pc);
};

} // namespace bbarm64
