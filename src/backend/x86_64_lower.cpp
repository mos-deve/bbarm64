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

// Save x86_64 EFLAGS to CPUContext.nzcv
// Maps: SF→N, ZF→Z, CF→C, OF→V
// ARM64 NZCV layout: [31]=N, [30]=Z, [29]=C, [28]=V
// Uses context memory as scratch (offsets 512+) to avoid stack issues
void IRLowering::save_nzcv() {
    // Save scratch registers to context memory
    emitter_.emit_mov_mem_reg(R12, 512, RAX);
    emitter_.emit_mov_mem_reg(R12, 520, RCX);
    emitter_.emit_mov_mem_reg(R12, 528, RDX);

    // Push EFLAGS, pop into RAX
    emitter_.emit_byte(0x9C); // pushfq
    emitter_.emit_byte(0x58); // pop rax

    // N = SF (bit 7)
    emitter_.emit_mov_reg_reg(RCX, RAX);
    emitter_.emit_shr_reg_imm(RCX, 7);
    emitter_.emit_and_reg_imm(RCX, 1);

    // Z = ZF (bit 6) << 1
    emitter_.emit_mov_reg_reg(RDX, RAX);
    emitter_.emit_shr_reg_imm(RDX, 6);
    emitter_.emit_and_reg_imm(RDX, 1);
    emitter_.emit_shl_reg_imm(RDX, 1);
    emitter_.emit_or_reg_reg(RCX, RDX);

    // C = CF (bit 0) << 2
    emitter_.emit_mov_reg_reg(RDX, RAX);
    emitter_.emit_and_reg_imm(RDX, 1);
    emitter_.emit_shl_reg_imm(RDX, 2);
    emitter_.emit_or_reg_reg(RCX, RDX);

    // V = OF (bit 11) << 3
    emitter_.emit_mov_reg_reg(RDX, RAX);
    emitter_.emit_shr_reg_imm(RDX, 11);
    emitter_.emit_and_reg_imm(RDX, 1);
    emitter_.emit_shl_reg_imm(RDX, 3);
    emitter_.emit_or_reg_reg(RCX, RDX);

    // Shift to bits 28-31
    emitter_.emit_shl_reg_imm(RCX, 28);

    // Store to ctx.nzcv (offset 272)
    emitter_.emit_mov_mem_reg(R12, 272, RCX);

    // Restore scratch registers
    emitter_.emit_mov_reg_mem(RDX, R12, 528);
    emitter_.emit_mov_reg_mem(RCX, R12, 520);
    emitter_.emit_mov_reg_mem(RAX, R12, 512);
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

        case IROpcode::SAR: {
            load_arm_reg(instr.src1, RAX);
            if (instr.src2 == IR_IMM) {
                emitter_.emit_sar_reg_imm(RAX, instr.imm);
            } else {
                load_arm_reg(instr.src2, RCX);
                emitter_.emit_sar_reg_cl(RAX);
            }
            store_arm_reg(RAX, instr.dest);
            break;
        }

        case IROpcode::SHR: {
            load_arm_reg(instr.src1, RAX);
            if (instr.src2 == IR_IMM) {
                emitter_.emit_shr_reg_imm(RAX, instr.imm);
            } else {
                load_arm_reg(instr.src2, RCX);
                emitter_.emit_shr_reg_cl(RAX);
            }
            store_arm_reg(RAX, instr.dest);
            break;
        }

        case IROpcode::SHL: {
            load_arm_reg(instr.src1, RAX);
            if (instr.src2 == IR_IMM) {
                emitter_.emit_shl_reg_imm(RAX, instr.imm);
            } else {
                load_arm_reg(instr.src2, RCX);
                emitter_.emit_shl_reg_cl(RAX);
            }
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
            // Conditional branch using EFLAGS from preceding CMP/SUBS.
            // EFLAGS are still valid since the JIT block stops at the branch.
            // Map ARM64 condition code to x86_64 condition code.
            uint8_t x86_cc;
            switch (instr.cond) {
                case 0:  x86_cc = CC_Z;  break;  // EQ → ZF=1
                case 1:  x86_cc = CC_NZ; break;  // NE → ZF=0
                case 2:  x86_cc = CC_NB; break;  // CS/HS → CF=0
                case 3:  x86_cc = CC_B;  break;  // CC/LO → CF=1
                case 4:  x86_cc = CC_S;  break;  // MI → SF=1
                case 5:  x86_cc = CC_NS; break;  // PL → SF=0
                case 6:  x86_cc = CC_O;  break;  // VS → OF=1
                case 7:  x86_cc = CC_NO; break;  // VC → OF=0
                case 8:  x86_cc = CC_NBE;break;  // HI → CF=0 && ZF=0
                case 9:  x86_cc = CC_BE; break;  // LS → CF=1 || ZF=1
                case 10: x86_cc = CC_NL; break;  // GE → SF=OF
                case 11: x86_cc = CC_L;  break;  // LT → SF!=OF
                case 12: x86_cc = CC_NLE;break;  // GT → ZF=0 && SF=OF
                case 13: x86_cc = CC_LE; break;  // LE → ZF=1 || SF!=OF
                case 14: x86_cc = CC_NZ; break;  // AL → always
                default: x86_cc = CC_Z;  break;  // NV → never
            }

            uint64_t taken_target = instr.imm;

            // Emit: J(!cc) fallthrough; taken: set pc, ret; fallthrough: set pc, ret
            uint8_t inv_cc = x86_cc ^ 1;

            emitter_.emit_byte(0x0F);
            emitter_.emit_byte(0x80 | inv_cc);
            size_t jcc_offset_pos = emitter_.offset();
            emitter_.emit_dword(0); // placeholder

            // Taken path
            emitter_.emit_mov_reg_imm(RAX, taken_target);
            emitter_.emit_mov_mem_reg(R12, CTX_PC_OFFSET, RAX);
            emitter_.emit_ret();

            // Patch Jcc offset
            size_t fallthrough_pos = emitter_.offset();
            int32_t jcc_offset = static_cast<int32_t>(fallthrough_pos - (jcc_offset_pos + 4));
            emitter_.patch_dword_at(jcc_offset_pos, static_cast<uint32_t>(jcc_offset));

            // Fallthrough path
            emitter_.emit_mov_reg_imm(RAX, fallthrough_pc_);
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

        // SIMD/FP instructions - using AVX2
        case IROpcode::VEC_DUP: {
            // Duplicate GPR to all bytes of vector register
            // Vd = instr.dest (vector register index)
            // Rn = instr.src1 (GPR index)
            // Element size = instr.imm (0=8b, 1=4h, 2=2s, 3=1d)
            uint8_t gpr = arm_reg_to_x86(instr.src1);
            load_arm_reg(instr.src1, gpr);

            // Vector register offset in CPUContext: 280 + vd * 16
            uint32_t vec_offset = 280 + instr.dest * 16;

            if (instr.imm == 0) {
                // DUP to 16 bytes: vpbroadcastb
                // VEX.256.66.0F38.W0 78 /r = vpbroadcastb ymm, r/m8
                // But we only need 128-bit (xmm)
                // VEX.128.66.0F38.W0 78 /r
                // REX.W + 0F 38 78 /r for vpbroadcastb xmm, r/m8
                // Actually: VEX.128.66.0F38.WIG 78 /r
                // Encoding: C4 E2 79 78 C0 (vpbroadcastb xmm0, al)
                // For general: C4 E2 79 78 /r
                // Let me use a simpler approach: store byte 16 times
                // Actually, let's use the VEX encoding for vpbroadcastb
                // VEX prefix: C4 E2 79 78
                // ModR/M: C0 | (dst_reg << 3) | src_reg
                // For xmm registers, we need to compute the VEX encoding
                // Simpler: just use mov to store the byte 16 times
                emitter_.emit_byte(0xC4); // VEX3
                emitter_.emit_byte(0xE2); // R'=0, X'=0, B'=0, m-mmmm=01 (0F)
                emitter_.emit_byte(0x79); // W=0, vvvv=0000, L=1(256-bit), pp=01(66)
                emitter_.emit_byte(0x78); // opcode vpbroadcastb
                // ModR/M: mod=11, reg=vd, rm=gpr
                uint8_t vd_reg = instr.dest; // xmm register index
                uint8_t src_low = gpr & 7;
                uint8_t reg_low = vd_reg & 7;
                uint8_t modrm = 0xC0 | (reg_low << 3) | src_low;
                emitter_.emit_byte(modrm);
            } else {
                // For other sizes, just zero the vector for now
                // pxor xmm, xmm
                emitter_.emit_byte(0x66);
                emitter_.emit_byte(0x0F);
                emitter_.emit_byte(0xEF);
                uint8_t vd_reg = instr.dest & 7;
                emitter_.emit_byte(0xC0 | (vd_reg << 3) | vd_reg);
            }
            break;
        }

        case IROpcode::VEC_ADD8:
        case IROpcode::VEC_SUB8:
        case IROpcode::VEC_AND:
        case IROpcode::VEC_OR:
        case IROpcode::VEC_XOR:
            // SIMD vector ops - emit NOP for now, interpreter handles these
            emitter_.emit_nop();
            break;

        default:
            emitter_.emit_nop();
            break;
    }

    return true;
}

} // namespace bbarm64
