#include "x86_64_lower.hpp"
#include "x86_64_emitter.hpp"
#include "x86_64_regalloc.hpp"

namespace bbarm64 {

IRLowering::IRLowering(X86_64Emitter& emitter, X86_64RegAlloc& regalloc)
    : emitter_(emitter), regalloc_(regalloc), fallthrough_pc_(0) {}

uint8_t IRLowering::arm_reg_to_x86(uint8_t arm_reg) {
    return static_cast<uint8_t>(X86_64RegAlloc::arm_to_x86_static(arm_reg));
}

void IRLowering::load_arm_reg(uint8_t arm_reg, uint8_t xreg) {
    int32_t offset = (arm_reg == 31) ? 248 : static_cast<int32_t>(arm_reg * 8);
    emitter_.emit_mov_reg_mem(xreg, R12, offset);
}

void IRLowering::store_arm_reg(uint8_t xreg, uint8_t arm_reg) {
    int32_t offset = (arm_reg == 31) ? 248 : static_cast<int32_t>(arm_reg * 8);
    emitter_.emit_mov_mem_reg(R12, offset, xreg);
}

void IRLowering::save_nzcv() {
    // TODO: Save EFLAGS to CPUContext.nzcv
}

void IRLowering::emit_block_ret(uint64_t target_pc) {
    emitter_.emit_mov_reg_imm(RAX, target_pc);
    emitter_.emit_mov_mem_reg(R12, CTX_PC_OFFSET, RAX);
    emitter_.emit_ret();
}

bool IRLowering::lower_block(const IRBlock& block, CPUContext& ctx, uint64_t fallthrough_pc) {
    (void)ctx;
    fallthrough_pc_ = fallthrough_pc;
    // Prologue: save ctx pointer (RDI) into R12
    emitter_.emit_mov_reg_reg(R12, RDI);
    for (const auto& instr : block.instructions) {
        if (!lower_instr(instr)) {
            return false;
        }
    }
    emit_block_ret(fallthrough_pc_);
    return true;
}

bool IRLowering::lower_instr(const IRInst& instr) {
    switch (instr.op) {
        case IROpcode::ADD: {
            load_arm_reg(instr.src1, RAX);
            if (instr.src2 == IR_IMM) {
                emitter_.emit_add_reg_imm(RAX, instr.imm);
            } else {
                load_arm_reg(instr.src2, RCX);
                emitter_.emit_add_reg_reg(RAX, RCX);
            }
            if (instr.flags_set) save_nzcv();
            store_arm_reg(RAX, instr.dest);
            break;
        }

        case IROpcode::SUB: {
            load_arm_reg(instr.src1, RAX);
            if (instr.src2 == IR_IMM) {
                emitter_.emit_sub_reg_imm(RAX, instr.imm);
            } else {
                load_arm_reg(instr.src2, RCX);
                emitter_.emit_sub_reg_reg(RAX, RCX);
            }
            if (instr.flags_set) save_nzcv();
            store_arm_reg(RAX, instr.dest);
            break;
        }

        case IROpcode::MUL: {
            load_arm_reg(instr.src1, RAX);
            load_arm_reg(instr.src2, RCX);
            emitter_.emit_imul_reg_reg(RAX, RCX);
            store_arm_reg(RAX, instr.dest);
            break;
        }

        case IROpcode::AND: {
            load_arm_reg(instr.src1, RAX);
            if (instr.src2 == IR_IMM) {
                emitter_.emit_and_reg_imm(RAX, instr.imm);
            } else {
                load_arm_reg(instr.src2, RCX);
                emitter_.emit_and_reg_reg(RAX, RCX);
            }
            if (instr.flags_set) save_nzcv();
            store_arm_reg(RAX, instr.dest);
            break;
        }

        case IROpcode::OR: {
            load_arm_reg(instr.src1, RAX);
            if (instr.src2 == IR_IMM) {
                emitter_.emit_or_reg_imm(RAX, instr.imm);
            } else {
                load_arm_reg(instr.src2, RCX);
                emitter_.emit_or_reg_reg(RAX, RCX);
            }
            store_arm_reg(RAX, instr.dest);
            break;
        }

        case IROpcode::XOR: {
            load_arm_reg(instr.src1, RAX);
            if (instr.src2 == IR_IMM) {
                emitter_.emit_xor_reg_imm(RAX, instr.imm);
            } else {
                load_arm_reg(instr.src2, RCX);
                emitter_.emit_xor_reg_reg(RAX, RCX);
            }
            store_arm_reg(RAX, instr.dest);
            break;
        }

        case IROpcode::CMP: {
            load_arm_reg(instr.src1, RAX);
            if (instr.src2 == IR_IMM || instr.src2 == IR_NONE) {
                emitter_.emit_cmp_reg_imm(RAX, instr.imm);
            } else {
                load_arm_reg(instr.src2, RCX);
                emitter_.emit_cmp_reg_reg(RAX, RCX);
            }
            if (instr.flags_set) save_nzcv();
            break;
        }

        case IROpcode::TST: {
            load_arm_reg(instr.src1, RAX);
            load_arm_reg(instr.src2, RCX);
            emitter_.emit_test_reg_reg(RAX, RCX);
            if (instr.flags_set) save_nzcv();
            break;
        }

        case IROpcode::MOV_IMM: {
            uint8_t dst = arm_reg_to_x86(instr.dest);
            emitter_.emit_mov_reg_imm(dst, instr.imm);
            store_arm_reg(dst, instr.dest);
            break;
        }

        case IROpcode::SET_REG: {
            if (instr.src1 == IR_NONE) {
                emitter_.emit_mov_reg_imm(RAX, instr.imm);
                store_arm_reg(RAX, instr.dest);
            } else {
                uint8_t src = arm_reg_to_x86(instr.src1);
                load_arm_reg(instr.src1, src);
                store_arm_reg(src, instr.dest);
            }
            break;
        }

        case IROpcode::LOAD64: {
            load_arm_reg(instr.src1, RAX);
            emitter_.emit_mov_reg_mem(RAX, RAX, instr.imm);
            store_arm_reg(RAX, instr.dest);
            break;
        }

        case IROpcode::LOAD32: {
            load_arm_reg(instr.src1, RAX);
            emitter_.emit_mov_reg_mem(RAX, RAX, instr.imm);
            store_arm_reg(RAX, instr.dest);
            break;
        }

        case IROpcode::STORE64: {
            load_arm_reg(instr.src1, RAX);
            load_arm_reg(instr.src2, RCX);
            emitter_.emit_mov_mem_reg(RAX, static_cast<int32_t>(instr.imm), RCX);
            break;
        }

        case IROpcode::STORE32: {
            load_arm_reg(instr.src1, RAX);
            load_arm_reg(instr.src2, RCX);
            emitter_.emit_mov_mem_reg(RAX, static_cast<int32_t>(instr.imm), RCX);
            break;
        }

        case IROpcode::JMP: {
            uint64_t target = instr.imm;
            emitter_.emit_mov_reg_imm(RAX, target);
            emitter_.emit_mov_mem_reg(R12, CTX_PC_OFFSET, RAX);
            emitter_.emit_ret();
            break;
        }

        case IROpcode::JMP_COND: {
            // For now, just set pc to fallthrough and return.
            // The run() loop will handle the actual condition check via interpreter.
            // TODO: Implement proper conditional branching with NZCV->EFLAGS mapping
            uint64_t target = fallthrough_pc_;
            emitter_.emit_mov_reg_imm(RAX, target);
            emitter_.emit_mov_mem_reg(R12, CTX_PC_OFFSET, RAX);
            emitter_.emit_ret();
            break;
        }

        case IROpcode::CALL: {
            uint64_t target = instr.imm;
            emitter_.emit_mov_reg_imm(RAX, target);
            emitter_.emit_mov_mem_reg(R12, CTX_PC_OFFSET, RAX);
            emitter_.emit_ret();
            break;
        }

        case IROpcode::RET: {
            emitter_.emit_mov_reg_mem(RAX, R12, 264);
            emitter_.emit_mov_mem_reg(R12, CTX_PC_OFFSET, RAX);
            emitter_.emit_ret();
            break;
        }

        case IROpcode::NOP:
        case IROpcode::BARRIER:
            emitter_.emit_nop();
            break;

        case IROpcode::SYSCALL:
            emitter_.emit_int3();
            break;

        case IROpcode::LOAD_PAIR: {
            load_arm_reg(instr.src1, RAX);
            emitter_.emit_mov_reg_mem(RCX, RAX, static_cast<int32_t>(instr.imm));
            emitter_.emit_mov_reg_mem(RDX, RAX, static_cast<int32_t>(instr.imm) + 8);
            store_arm_reg(RCX, instr.dest);
            store_arm_reg(RDX, instr.src2);
            break;
        }

        case IROpcode::STORE_PAIR: {
            load_arm_reg(instr.src1, RAX);
            load_arm_reg(instr.src2, RCX);
            load_arm_reg(static_cast<uint8_t>(instr.imm >> 32), RDX);
            emitter_.emit_mov_mem_reg(RAX, static_cast<int32_t>(instr.imm), RCX);
            emitter_.emit_mov_mem_reg(RAX, static_cast<int32_t>(instr.imm) + 8, RDX);
            break;
        }

        default:
            emitter_.emit_nop();
            break;
    }

    return true;
}

} // namespace bbarm64
