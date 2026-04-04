#include "ir.hpp"
#include "../decoder/arm64_decoder.hpp"

namespace bbarm64 {

IRBuilder::IRBuilder() : next_reg_(1) {}

IRReg IRBuilder::alloc_reg(IRBlock& block) {
    (void)block;
    return next_reg_++;
}

bool IRBuilder::build(const DecodedInstr* instructions, uint32_t count,
                       uint64_t start_pc, IRBlock& out) {
    out.arm_pc_start = start_pc;
    out.arm_instr_count = count;
    out.is_conditional = false;
    next_reg_ = 1;

    for (uint32_t i = 0; i < count; i++) {
        if (!build_instr(instructions[i], start_pc + i * 4, out)) {
            return false;
        }
        if (instructions[i].category == InstrCategory::BRANCH ||
            instructions[i].category == InstrCategory::BRANCH_COND ||
            instructions[i].category == InstrCategory::SYSCALL) {
            out.arm_instr_count = i + 1;
            break;
        }
    }

    out.arm_pc_end = start_pc + out.arm_instr_count * 4;
    return true;
}

bool IRBuilder::build_instr(const DecodedInstr& instr, uint64_t pc, IRBlock& block) {
    IRInst ir;
    ir.size = instr.size ? 8 : 4;

    switch (instr.opcode) {
        case 0x00:
            ir.op = IROpcode::ADD;
            ir.dest = instr.rd;
            ir.src1 = instr.rn;
            if (instr.rm == 0xFF) {
                ir.src2 = IR_IMM;
                ir.imm = instr.imm;
            } else {
                ir.src2 = instr.rm;
            }
            ir.flags_set = instr.sets_flags;
            block.instructions.push_back(ir);
            break;

        case 0x01:
            ir.op = IROpcode::SUB;
            ir.dest = instr.rd;
            ir.src1 = instr.rn;
            if (instr.rm == 0xFF) {
                ir.src2 = IR_IMM;
                ir.imm = instr.imm;
            } else {
                ir.src2 = instr.rm;
            }
            ir.flags_set = instr.sets_flags;
            block.instructions.push_back(ir);
            break;

        case 0x04: // ASR (arithmetic shift right)
            ir.op = IROpcode::SAR;
            ir.dest = instr.rd;
            ir.src1 = instr.rn;
            ir.src2 = IR_IMM;
            ir.imm = instr.imm;
            block.instructions.push_back(ir);
            break;

        case 0x05: // LSR (logical shift right)
            ir.op = IROpcode::SHR;
            ir.dest = instr.rd;
            ir.src1 = instr.rn;
            ir.src2 = IR_IMM;
            ir.imm = instr.imm;
            block.instructions.push_back(ir);
            break;

        case 0x06: // LSL (logical shift left)
            ir.op = IROpcode::SHL;
            ir.dest = instr.rd;
            ir.src1 = instr.rn;
            ir.src2 = IR_IMM;
            ir.imm = instr.imm;
            block.instructions.push_back(ir);
            break;

        case 0x20:
            ir.op = IROpcode::SET_REG;
            ir.dest = instr.rd;
            ir.src1 = instr.rm;
            block.instructions.push_back(ir);
            break;

        case 0x21:
        case 0x22:
        case 0x23:
            ir.op = IROpcode::MOV_IMM;
            ir.dest = instr.rd;
            ir.imm = instr.imm;
            block.instructions.push_back(ir);
            break;

        case 0x24: {
            // ADR: absolute address = pc + sign_extended_offset
            ir.op = IROpcode::MOV_IMM;
            ir.dest = instr.rd;
            ir.imm = static_cast<uint64_t>(static_cast<int64_t>(pc) + instr.imm_s);
            block.instructions.push_back(ir);
            break;
        }

        case 0x25: {
            // ADRP: absolute address = (pc & ~0xFFF) + (sign_extended_offset << 12)
            ir.op = IROpcode::MOV_IMM;
            ir.dest = instr.rd;
            ir.imm = (pc & ~0xFFFULL) + (static_cast<uint64_t>(instr.imm_s) << 12);
            block.instructions.push_back(ir);
            break;
        }

        case 0x30:
            ir.op = IROpcode::CMP;
            ir.src1 = instr.rn;
            ir.src2 = instr.rm;
            ir.flags_set = true;
            block.instructions.push_back(ir);
            break;

        case 0x31:
            ir.op = IROpcode::TST;
            ir.src1 = instr.rn;
            ir.src2 = instr.rm;
            ir.flags_set = true;
            block.instructions.push_back(ir);
            break;

        case 0x50: {
            ir.op = IROpcode::JMP;
            // Sign-extend branch_offset from uint32_t to int64_t
            int64_t offset = static_cast<int64_t>(static_cast<int32_t>(instr.branch_offset));
            ir.imm = static_cast<uint64_t>(static_cast<int64_t>(pc) + offset);
            block.instructions.push_back(ir);
            break;
        }

        case 0x51: {
            IRInst save_lr;
            save_lr.op = IROpcode::SET_REG;
            save_lr.dest = 30;
            save_lr.src1 = IR_NONE;
            save_lr.imm = pc + 4;
            block.instructions.push_back(save_lr);

            ir.op = IROpcode::CALL;
            int64_t offset = static_cast<int64_t>(static_cast<int32_t>(instr.branch_offset));
            ir.imm = static_cast<uint64_t>(static_cast<int64_t>(pc) + offset);
            block.instructions.push_back(ir);
            break;
        }

        case 0x53: {
            ir.op = IROpcode::RET;
            ir.src1 = 30;
            block.instructions.push_back(ir);
            break;
        }

        case 0x54: {
            ir.op = IROpcode::JMP_COND;
            ir.flags_used = true;
            ir.imm = pc + instr.branch_offset;
            ir.cond = instr.cond;
            block.is_conditional = true;
            block.branch_target_taken = pc + instr.branch_offset;
            block.branch_target_fallthrough = pc + 4;
            block.instructions.push_back(ir);
            break;
        }

        case 0x55: {
            ir.op = IROpcode::CMP;
            ir.src1 = instr.rn;
            ir.src2 = IR_NONE;
            ir.imm = 0;
            ir.flags_set = true;
            block.instructions.push_back(ir);

            IRInst jmp;
            jmp.op = IROpcode::JMP_COND;
            jmp.flags_used = true;
            jmp.imm = pc + instr.branch_offset;
            jmp.cond = 0;
            block.is_conditional = true;
            block.instructions.push_back(jmp);
            break;
        }

        case 0x56: {
            ir.op = IROpcode::CMP;
            ir.src1 = instr.rn;
            ir.src2 = IR_NONE;
            ir.imm = 0;
            ir.flags_set = true;
            block.instructions.push_back(ir);

            IRInst jmp;
            jmp.op = IROpcode::JMP_COND;
            jmp.flags_used = true;
            jmp.imm = pc + instr.branch_offset;
            jmp.cond = 1;
            block.is_conditional = true;
            block.instructions.push_back(jmp);
            break;
        }

        case 0x63: {
            ir.op = IROpcode::LOAD64;
            ir.dest = instr.rt;
            ir.src1 = instr.rn;
            ir.imm = instr.imm;
            block.instructions.push_back(ir);
            break;
        }

        case 0x62: {
            ir.op = IROpcode::LOAD32;
            ir.dest = instr.rt;
            ir.src1 = instr.rn;
            ir.imm = instr.imm;
            block.instructions.push_back(ir);
            break;
        }

        case 0x73: {
            ir.op = IROpcode::STORE64;
            ir.src1 = instr.rn;
            ir.src2 = instr.rt;
            ir.imm = instr.imm;
            block.instructions.push_back(ir);
            break;
        }

        case 0x72: {
            ir.op = IROpcode::STORE32;
            ir.src1 = instr.rn;
            ir.src2 = instr.rt;
            ir.imm = instr.imm;
            block.instructions.push_back(ir);
            break;
        }

        case 0x80: {
            ir.op = IROpcode::LOAD_PAIR;
            ir.dest = instr.rt;
            ir.src1 = instr.rn;
            ir.src2 = instr.rm;
            ir.imm = instr.imm_s;
            block.instructions.push_back(ir);
            break;
        }

        case 0x81: {
            ir.op = IROpcode::STORE_PAIR;
            ir.src1 = instr.rn;
            ir.src2 = instr.rt;
            ir.imm = instr.imm_s;
            block.instructions.push_back(ir);
            break;
        }

        case 0x90: {
            ir.op = IROpcode::NOP;
            block.instructions.push_back(ir);
            break;
        }

        case 0x91: case 0x92: case 0x93: {
            ir.op = IROpcode::BARRIER;
            block.instructions.push_back(ir);
            break;
        }

        case 0xFF: {
            ir.op = IROpcode::SYSCALL;
            ir.imm = instr.imm;
            block.instructions.push_back(ir);
            break;
        }

        // SIMD/FP instructions
        case 0xA0: { // VEC_DUP
            ir.op = IROpcode::VEC_DUP;
            ir.dest = instr.rd; // vector register index
            ir.src1 = instr.rm; // GPR index
            ir.imm = instr.size; // element size
            block.instructions.push_back(ir);
            break;
        }

        case 0xA1: { // VEC_ADD
            ir.op = IROpcode::VEC_ADD8;
            ir.dest = instr.rd;
            ir.src1 = instr.rn;
            ir.src2 = instr.rm;
            block.instructions.push_back(ir);
            break;
        }

        case 0xA2: { // VEC_SUB
            ir.op = IROpcode::VEC_SUB8;
            ir.dest = instr.rd;
            ir.src1 = instr.rn;
            ir.src2 = instr.rm;
            block.instructions.push_back(ir);
            break;
        }

        case 0xA3: { // VEC_AND
            ir.op = IROpcode::VEC_AND;
            ir.dest = instr.rd;
            ir.src1 = instr.rn;
            ir.src2 = instr.rm;
            block.instructions.push_back(ir);
            break;
        }

        case 0xA4: { // VEC_OR
            ir.op = IROpcode::VEC_OR;
            ir.dest = instr.rd;
            ir.src1 = instr.rn;
            ir.src2 = instr.rm;
            block.instructions.push_back(ir);
            break;
        }

        case 0xA5: { // VEC_XOR
            ir.op = IROpcode::VEC_XOR;
            ir.dest = instr.rd;
            ir.src1 = instr.rn;
            ir.src2 = instr.rm;
            block.instructions.push_back(ir);
            break;
        }

        case 0xA6: { // VEC_BIC
            ir.op = IROpcode::VEC_AND; // BIC = AND NOT, approximate with AND for now
            ir.dest = instr.rd;
            ir.src1 = instr.rn;
            ir.src2 = instr.rm;
            block.instructions.push_back(ir);
            break;
        }

        default:
            ir.op = IROpcode::NOP;
            block.instructions.push_back(ir);
            break;
    }

    return true;
}

} // namespace bbarm64
