#include "arm64_decoder.hpp"
#include <cstring>

namespace bbarm64 {

ARM64Decoder::ARM64Decoder() {}

uint32_t ARM64Decoder::bits(uint32_t val, int hi, int lo) const {
    return (val >> lo) & ((1u << (hi - lo + 1)) - 1);
}

int32_t ARM64Decoder::sign_extend(uint64_t val, int width) const {
    uint64_t mask = (1ULL << width) - 1;
    uint64_t result = val & mask;
    if (result & (1ULL << (width - 1))) {
        result |= ~mask;
    }
    return static_cast<int32_t>(result);
}

bool ARM64Decoder::decode(uint32_t instr, DecodedInstr& out) {
    std::memset(&out, 0, sizeof(out));
    out.raw = instr;

    uint32_t op1 = bits(instr, 31, 29);
    uint32_t op25 = bits(instr, 31, 25);

    // ADR: bits[31:24]=0b00010000
    if (bits(instr, 31, 24) == 0b00010000) {
        return decode_pcrel(instr, out);
    }

    // ADRP: bits[31:24]=0b10010000 or 0b10110000 or 0b11010000 or 0b11110000
    uint32_t b31_24 = bits(instr, 31, 24);
    if (b31_24 == 0b10010000 || b31_24 == 0b10110000 || b31_24 == 0b11010000 || b31_24 == 0b11110000) {
        return decode_pcrel(instr, out);
    }

    // LDR (literal): bits[31:26]=0b010110
    if (bits(instr, 31, 26) == 0b010110) {
        return decode_pcrel(instr, out);
    }

    // TBZ/TBNZ: bits[31:25]=0b00110110 or 0b00110111
    if (op25 == 0b00110110 || op25 == 0b00110111) {
        return decode_test_branch(instr, out);
    }

    // Check for data processing register instructions first (op1 can be 0b101 for sf=1)
    uint32_t op28_24 = bits(instr, 28, 24);
    if (op1 == 0b101 && (op28_24 <= 0b01011 || op28_24 == 0b10000 || op28_24 == 0b10001 ||
                          op28_24 == 0b10010 || op28_24 == 0b10011 || op28_24 == 0b11000 ||
                          op28_24 == 0b11001)) {
        return decode_data_proc_reg(instr, out);
    }

    if (op1 == 0b101) {
        uint32_t op2 = bits(instr, 31, 29);
        if (op2 == 0b101) {
            return decode_branch(instr, out);
        }
        if (bits(instr, 31, 21) == 0b11010100001) {
            out.category = InstrCategory::SYSCALL;
            out.opcode = bits(instr, 20, 5);
            out.imm = bits(instr, 20, 5);
            return true;
        }
        return decode_system(instr, out);
    }

    // B/BL: bits[31:26] = 0b000101 or 0b100101
    if (bits(instr, 31, 26) == 0b000101 || bits(instr, 31, 26) == 0b100101) {
        return decode_branch(instr, out);
    }

    // Logical shifted register (AND, ORR, EOR, etc.): check bits[28:24] pattern
    uint32_t b28_24_check = bits(instr, 28, 24);
    if ((b28_24_check <= 0b01011 || b28_24_check == 0b10000 || b28_24_check == 0b10001 ||
         b28_24_check == 0b10010 || b28_24_check == 0b10011 || b28_24_check == 0b11000 ||
         b28_24_check == 0b11001) && op1 != 0b101) {
        if (decode_data_proc_reg(instr, out)) return true;
    }

    // MOVZ/MOVK/MOVN: bits[28:23] = 0b100101 (check before load_store routing)
    if (bits(instr, 28, 23) == 0b100101) {
        return decode_data_proc_imm(instr, out);
    }

    // MOVZ/MOVK/MOVN: bits[28:23] = 0b100101 (check before load_store routing)
    if (bits(instr, 28, 23) == 0b100101) {
        return decode_data_proc_imm(instr, out);
    }

    if (op1 == 0b001 || op1 == 0b010 || op1 == 0b011 || op1 == 0b111) {
        return decode_load_store(instr, out);
    }

    if (op1 == 0b000 || op1 == 0b010 || op1 == 0b100 || op1 == 0b110) {
        uint32_t op = bits(instr, 30, 29);
        if (op == 0b00 || op == 0b01) {
            if (decode_data_proc_reg(instr, out)) return true;
        }
        if (op == 0b10) {
            if (decode_data_proc_imm(instr, out)) return true;
        }
    }

    // MOVZ/MOVK/MOVN: bits[31:29] = 0b110, bits[28:23] = 0b100101
    if (op1 == 0b110 && bits(instr, 28, 23) == 0b100101) {
        return decode_data_proc_imm(instr, out);
    }

    // ADD/SUB (immediate): bits[30:24] = 0b0010001 (ADD) or 0b1010001 (SUB)
    if (bits(instr, 30, 24) == 0b0010001 || bits(instr, 30, 24) == 0b1010001) {
        return decode_data_proc_imm(instr, out);
    }

    // System instructions (NOP, DMB, DSB, ISB, MRS, MSR): bits[31:25] = 0b1101010
    if (op25 == 0b1101010) {
        return decode_system(instr, out);
    }

    out.category = InstrCategory::UNKNOWN;
    return false;
}

bool ARM64Decoder::decode_data_proc_reg(uint32_t instr, DecodedInstr& out) {
    uint32_t sf = bits(instr, 31, 31);
    out.size = sf;
    uint32_t op = bits(instr, 30, 21);

    if (op == 0x02C || op == 0x2AC) {
        out.category = InstrCategory::INTEGER_ARITH;
        out.opcode = 0x00;
        out.rd = bits(instr, 4, 0);
        out.rn = bits(instr, 9, 5);
        out.rm = bits(instr, 20, 16);
        out.shift_type = bits(instr, 23, 22);
        out.shift_amount = bits(instr, 15, 10);
        out.sets_flags = bits(instr, 31, 29) & 0x1;
        return true;
    }

    if (op == 0x034 || op == 0x234) {
        out.category = InstrCategory::INTEGER_ARITH;
        out.opcode = 0x01;
        out.rd = bits(instr, 4, 0);
        out.rn = bits(instr, 9, 5);
        out.rm = bits(instr, 20, 16);
        out.shift_type = bits(instr, 23, 22);
        out.shift_amount = bits(instr, 15, 10);
        out.sets_flags = bits(instr, 31, 29) & 0x1;
        return true;
    }

    uint32_t ands_op = bits(instr, 30, 29);
    uint32_t ands_op2 = bits(instr, 28, 24);
    if ((ands_op == 0b00 || ands_op == 0b01 || ands_op == 0b10) &&
        (ands_op2 == 0b00000 || ands_op2 == 0b00001 ||
         ands_op2 == 0b00010 || ands_op2 == 0b00011 ||
         ands_op2 == 0b01000 || ands_op2 == 0b01001 ||
         ands_op2 == 0b01010 || ands_op2 == 0b01011)) {
        out.category = InstrCategory::LOGICAL;
        out.rd = bits(instr, 4, 0);
        out.rn = bits(instr, 9, 5);
        out.rm = bits(instr, 20, 16);
        out.shift_type = bits(instr, 23, 22);
        out.shift_amount = bits(instr, 15, 10);
        // Combined opcode from ands_op (bits[30:29]) and ands_op2 (bits[28:24])
        uint32_t combined = (ands_op << 5) | ands_op2;
        if (combined == 0b0000000) out.opcode = 0x10;  // AND
        else if (combined == 0b0000001) out.opcode = 0x11;  // BIC
        else if (combined == 0b0101010) out.opcode = 0x12;  // ORR
        else if (combined == 0b0101011) out.opcode = 0x13;  // ORN
        else if (combined == 0b1001000) out.opcode = 0x14;  // EOR
        else if (combined == 0b1001001) out.opcode = 0x15;  // EON
        else if (combined == 0b0001010) out.opcode = 0x16;  // ANDS
        else if (combined == 0b0001011) out.opcode = 0x17;  // BICS
        else { out.opcode = 0x10; }  // Default to AND
        out.sets_flags = (out.opcode == 0x16 || out.opcode == 0x17);
        return true;
    }

    if (bits(instr, 30, 21) == 0x2A0 && bits(instr, 9, 5) == 31) {
        out.category = InstrCategory::MOVE;
        out.opcode = 0x20;
        out.rd = bits(instr, 4, 0);
        out.rn = 31;
        out.rm = bits(instr, 20, 16);
        return true;
    }

    if (bits(instr, 30, 21) == 0x1B0) {
        out.category = InstrCategory::INTEGER_ARITH;
        out.rd = bits(instr, 4, 0);
        out.rn = bits(instr, 9, 5);
        out.rm = bits(instr, 20, 16);
        uint32_t ra = bits(instr, 15, 10);
        if (ra == 31) out.opcode = 0x02;
        else out.opcode = 0x03;
        return true;
    }

    if (bits(instr, 30, 21) == 0x1AC) {
        out.category = InstrCategory::INTEGER_ARITH;
        out.rd = bits(instr, 4, 0);
        out.rn = bits(instr, 9, 5);
        out.rm = bits(instr, 20, 16);
        out.opcode = bits(instr, 11, 10) == 1 ? 0x05 : 0x04;
        return true;
    }

    if (bits(instr, 30, 22) == 0x0D3) {
        out.category = InstrCategory::SHIFT;
        out.rd = bits(instr, 4, 0);
        out.rn = bits(instr, 9, 5);
        out.rm = bits(instr, 20, 16);
        out.opcode = bits(instr, 23, 22);
        return true;
    }

    if (bits(instr, 30, 21) == 0x2B0 && bits(instr, 4, 0) == 31) {
        out.category = InstrCategory::COMPARE;
        out.opcode = 0x30;
        out.rn = bits(instr, 9, 5);
        out.rm = bits(instr, 20, 16);
        out.sets_flags = true;
        return true;
    }

    if (bits(instr, 30, 21) == 0x2A0 && bits(instr, 4, 0) == 31 &&
        bits(instr, 28, 24) == 0b01010) {
        out.category = InstrCategory::COMPARE;
        out.opcode = 0x31;
        out.rn = bits(instr, 9, 5);
        out.rm = bits(instr, 20, 16);
        out.sets_flags = true;
        return true;
    }

    if (bits(instr, 30, 24) == 0x1A8) {
        out.category = InstrCategory::BRANCH_COND;
        out.opcode = 0x40;
        out.rd = bits(instr, 4, 0);
        out.rn = bits(instr, 9, 5);
        out.rm = bits(instr, 20, 16);
        out.cond = bits(instr, 15, 12);
        return true;
    }

    out.category = InstrCategory::UNKNOWN;
    return false;
}

bool ARM64Decoder::decode_data_proc_imm(uint32_t instr, DecodedInstr& out) {
    uint32_t sf = bits(instr, 31, 31);
    out.size = sf;

    // ADD (immediate): bits[30:24]=0b0010001
    if (bits(instr, 30, 24) == 0b0010001) {
        out.category = InstrCategory::INTEGER_ARITH;
        out.opcode = 0x00;
        out.rd = bits(instr, 4, 0);
        out.rn = bits(instr, 9, 5);
        out.rm = 0xFF;
        out.imm = bits(instr, 21, 10);
        if (bits(instr, 22, 22)) out.imm <<= 12;
        out.sets_flags = bits(instr, 30, 30);
        return true;
    }

    // SUB (immediate): bits[30:24]=0b1010001
    if (bits(instr, 30, 24) == 0b1010001) {
        out.category = InstrCategory::INTEGER_ARITH;
        out.opcode = 0x01;
        out.rd = bits(instr, 4, 0);
        out.rn = bits(instr, 9, 5);
        out.rm = 0xFF;
        out.imm = bits(instr, 21, 10);
        if (bits(instr, 22, 22)) out.imm <<= 12;
        out.sets_flags = bits(instr, 30, 30);
        return true;
    }

    uint32_t opc = bits(instr, 30, 29);
    if (bits(instr, 28, 23) == 0x25) {
        out.category = InstrCategory::MOVE;
        out.rd = bits(instr, 4, 0);
        out.imm = bits(instr, 20, 5);
        if (opc == 0b00) out.opcode = 0x21;
        else if (opc == 0b10) out.opcode = 0x22;
        else if (opc == 0b11) out.opcode = 0x23;
        return true;
    }

    out.category = InstrCategory::UNKNOWN;
    return false;
}

bool ARM64Decoder::decode_branch(uint32_t instr, DecodedInstr& out) {
    if (bits(instr, 31, 26) == 0b000101) {
        out.category = InstrCategory::BRANCH;
        out.opcode = 0x50;
        int32_t offset = sign_extend(instr, 26);
        out.branch_offset = offset << 2;
        return true;
    }

    if (bits(instr, 31, 26) == 0b100101) {
        out.category = InstrCategory::BRANCH;
        out.opcode = 0x51;
        int32_t offset = sign_extend(instr, 26);
        out.branch_offset = offset << 2;
        return true;
    }

    if (bits(instr, 31, 21) == 0b11010110000 && bits(instr, 9, 0) == 0) {
        out.category = InstrCategory::BRANCH;
        out.opcode = 0x53;
        out.rn = bits(instr, 9, 5);
        return true;
    }

    if (bits(instr, 31, 21) == 0b11010110000 && bits(instr, 9, 0) == 0b000000) {
        out.category = InstrCategory::BRANCH;
        out.opcode = 0x52;
        out.rn = bits(instr, 9, 5);
        return true;
    }

    if (bits(instr, 31, 25) == 0b0101010) {
        out.category = InstrCategory::BRANCH_COND;
        out.opcode = 0x54;
        out.cond = bits(instr, 3, 0);
        int32_t offset = sign_extend(instr, 19);
        out.branch_offset = offset << 2;
        return true;
    }

    if (bits(instr, 31, 25) == 0b0011010 || bits(instr, 31, 25) == 0b1011010) {
        out.category = InstrCategory::BRANCH_COND;
        out.opcode = bits(instr, 24, 24) ? 0x56 : 0x55;
        out.rn = bits(instr, 9, 5);
        out.size = bits(instr, 31, 31);
        int32_t offset = sign_extend(instr, 19);
        out.branch_offset = offset << 2;
        return true;
    }

    out.category = InstrCategory::UNKNOWN;
    return false;
}

bool ARM64Decoder::decode_load_store(uint32_t instr, DecodedInstr& out) {
    uint32_t opc = bits(instr, 31, 30);
    uint32_t b28_26 = bits(instr, 28, 26);

    // Load/store unsigned immediate: bits[28:26] = 0b111
    if (b28_26 == 0b111) {
        out.category = InstrCategory::LOAD_STORE;
        out.rt = bits(instr, 4, 0);
        out.rn = bits(instr, 9, 5);
        uint32_t imm12 = bits(instr, 21, 10);
        out.imm = imm12 << opc;
        bool is_load = bits(instr, 22, 22);
        if (is_load) {
            if (opc == 0b11) out.opcode = 0x63;
            else if (opc == 0b10) out.opcode = 0x62;
            else if (opc == 0b01) out.opcode = 0x61;
            else out.opcode = 0x60;
        } else {
            if (opc == 0b11) out.opcode = 0x73;
            else if (opc == 0b10) out.opcode = 0x72;
            else if (opc == 0b01) out.opcode = 0x71;
            else out.opcode = 0x70;
        }
        return true;
    }

    // LDR/STR (register offset): bits[28:26] = 0b010
    if (b28_26 == 0b010) {
        out.category = InstrCategory::LOAD_STORE;
        out.rt = bits(instr, 4, 0);
        out.rn = bits(instr, 9, 5);
        out.rm = bits(instr, 20, 16);
        out.shift_type = bits(instr, 13, 12);
        out.imm = 0;
        bool is_load = bits(instr, 22, 22);
        if (is_load) {
            if (opc == 0b11) out.opcode = 0x63;
            else if (opc == 0b10) out.opcode = 0x62;
            else if (opc == 0b01) out.opcode = 0x61;
            else out.opcode = 0x60;
        } else {
            if (opc == 0b11) out.opcode = 0x73;
            else if (opc == 0b10) out.opcode = 0x72;
            else if (opc == 0b01) out.opcode = 0x71;
            else out.opcode = 0x70;
        }
        return true;
    }

    // LDR/STR (register): bits[28:26] = 0b110
    if (b28_26 == 0b110) {
        out.category = InstrCategory::LOAD_STORE;
        out.rt = bits(instr, 4, 0);
        out.rn = bits(instr, 9, 5);
        out.rm = bits(instr, 20, 16);
        out.shift_type = bits(instr, 13, 12);
        out.imm = 0;
        bool is_load = bits(instr, 22, 22);
        if (is_load) {
            if (opc == 0b11) out.opcode = 0x63;
            else if (opc == 0b10) out.opcode = 0x62;
            else if (opc == 0b01) out.opcode = 0x61;
            else out.opcode = 0x60;
        } else {
            if (opc == 0b11) out.opcode = 0x73;
            else if (opc == 0b10) out.opcode = 0x72;
            else if (opc == 0b01) out.opcode = 0x71;
            else out.opcode = 0x70;
        }
        return true;
    }

    // LDR/STR (register): bits[28:26] = 0b110
    if (b28_26 == 0b110) {
        out.category = InstrCategory::LOAD_STORE;
        out.rt = bits(instr, 4, 0);
        out.rn = bits(instr, 9, 5);
        out.rm = bits(instr, 20, 16);
        out.shift_type = bits(instr, 13, 12);
        out.imm = 0;
        bool is_load = bits(instr, 22, 22);
        if (is_load) {
            if (opc == 0b11) out.opcode = 0x63;
            else if (opc == 0b10) out.opcode = 0x62;
            else if (opc == 0b01) out.opcode = 0x61;
            else out.opcode = 0x60;
        } else {
            if (opc == 0b11) out.opcode = 0x73;
            else if (opc == 0b10) out.opcode = 0x72;
            else if (opc == 0b01) out.opcode = 0x71;
            else out.opcode = 0x70;
        }
        return true;
    }

    // LDR/STR (pre/post index): bits[28:26] = 0b011
    if (b28_26 == 0b011) {
        out.category = InstrCategory::LOAD_STORE;
        out.rt = bits(instr, 4, 0);
        out.rn = bits(instr, 9, 5);
        out.rm = 255;
        int32_t imm9 = sign_extend(bits(instr, 20, 12), 9);
        out.imm_s = imm9;
        out.imm = imm9;
        bool is_load = bits(instr, 22, 22);
        if (is_load) {
            if (opc == 0b11) out.opcode = 0x63;
            else if (opc == 0b10) out.opcode = 0x62;
            else if (opc == 0b01) out.opcode = 0x61;
            else out.opcode = 0x60;
        } else {
            if (opc == 0b11) out.opcode = 0x73;
            else if (opc == 0b10) out.opcode = 0x72;
            else if (opc == 0b01) out.opcode = 0x71;
            else out.opcode = 0x70;
        }
        return true;
    }

    // LDP/STP
    if (bits(instr, 31, 25) == 0b0010100) {
        out.category = InstrCategory::LOAD_STORE_PAIR;
        out.rt = bits(instr, 4, 0);
        out.rn = bits(instr, 9, 5);
        out.rm = bits(instr, 14, 10);
        out.imm = bits(instr, 21, 15);
        out.imm_s = sign_extend(out.imm, 7);
        out.imm_s <<= 3;
        bool is_load = bits(instr, 22, 22);
        out.opcode = is_load ? 0x80 : 0x81;
        return true;
    }

    out.category = InstrCategory::UNKNOWN;
    return false;
}

bool ARM64Decoder::decode_system(uint32_t instr, DecodedInstr& out) {
    if (instr == 0xD503201F) {
        out.category = InstrCategory::SYSTEM;
        out.opcode = 0x90;
        return true;
    }

    if (bits(instr, 31, 22) == 0b1101010100) {
        out.category = InstrCategory::SYSTEM;
        uint32_t op1 = bits(instr, 11, 8);
        if (op1 == 0b0101) out.opcode = 0x91;
        else if (op1 == 0b0110) out.opcode = 0x92;
        else if (op1 == 0b0111) out.opcode = 0x93;
        else if (op1 == 0b0000) out.opcode = 0x94;
        else if (op1 == 0b0001) out.opcode = 0x95;
        else { out.opcode = 0x90; }
        out.rd = bits(instr, 4, 0);
        return true;
    }

    out.category = InstrCategory::UNKNOWN;
    return false;
}

bool ARM64Decoder::decode_pcrel(uint32_t instr, DecodedInstr& out) {
    if (bits(instr, 31, 24) == 0b00010000) {
        out.category = InstrCategory::MOVE;
        out.opcode = 0x24;
        out.rd = bits(instr, 4, 0);
        uint32_t immhi = bits(instr, 23, 5);
        uint32_t immlo = bits(instr, 30, 29);
        uint64_t imm21 = (immhi << 2) | immlo;
        out.imm_s = sign_extend(imm21, 21);
        out.imm = static_cast<uint64_t>(out.imm_s);
        out.is_pc_relative = true;
        return true;
    }

    if (bits(instr, 31, 24) == 0b10010000 || bits(instr, 31, 24) == 0b10110000 || bits(instr, 31, 24) == 0b11010000 || bits(instr, 31, 24) == 0b11110000) {
        out.category = InstrCategory::MOVE;
        out.opcode = 0x25;
        out.rd = bits(instr, 4, 0);
        uint32_t immhi = bits(instr, 23, 5);
        uint32_t immlo = bits(instr, 30, 29);
        uint64_t imm21 = (immhi << 2) | immlo;
        out.imm_s = sign_extend(imm21, 21);
        out.imm = static_cast<uint64_t>(out.imm_s);
        out.is_pc_relative = true;
        return true;
    }

    if (bits(instr, 31, 24) == 0b10010000 || bits(instr, 31, 24) == 0b10110000 || bits(instr, 31, 24) == 0b11010000 || bits(instr, 31, 24) == 0b11110000) {
        out.category = InstrCategory::MOVE;
        out.opcode = 0x25;
        out.rd = bits(instr, 4, 0);
        uint32_t immhi = bits(instr, 23, 5);
        uint32_t immlo = bits(instr, 30, 29);
        uint64_t imm21 = (immhi << 2) | immlo;
        out.imm_s = sign_extend(imm21, 21);
        out.imm = static_cast<uint64_t>(out.imm_s);
        out.is_pc_relative = true;
        return true;
    }

    if (bits(instr, 31, 26) == 0b010110) {
        out.category = InstrCategory::LOAD_STORE;
        out.rt = bits(instr, 4, 0);
        out.rn = 31;
        uint32_t imm19 = bits(instr, 23, 5);
        out.imm_s = sign_extend(imm19, 19);
        out.is_pc_relative = true;
        uint32_t opc = bits(instr, 31, 30);
        if (opc == 0b00) out.opcode = 0x62;
        else if (opc == 0b01) out.opcode = 0x63;
        else { out.category = InstrCategory::SIMD_FP; return true; }
        return true;
    }

    out.category = InstrCategory::UNKNOWN;
    return false;
}

bool ARM64Decoder::decode_test_branch(uint32_t instr, DecodedInstr& out) {
    out.category = InstrCategory::BRANCH_COND;
    out.rn = bits(instr, 4, 0);
    out.size = bits(instr, 31, 31);
    out.cond = bits(instr, 24, 24);
    out.opcode = out.cond ? 0x58 : 0x57;
    out.imm = bits(instr, 23, 19);
    int32_t offset = sign_extend(bits(instr, 18, 5), 14);
    out.branch_offset = offset << 2;
    return true;
}

const char* ARM64Decoder::category_name(InstrCategory cat) {
    switch (cat) {
        case InstrCategory::INTEGER_ARITH: return "INTEGER_ARITH";
        case InstrCategory::LOGICAL: return "LOGICAL";
        case InstrCategory::SHIFT: return "SHIFT";
        case InstrCategory::BRANCH: return "BRANCH";
        case InstrCategory::BRANCH_COND: return "BRANCH_COND";
        case InstrCategory::LOAD_STORE: return "LOAD_STORE";
        case InstrCategory::LOAD_STORE_PAIR: return "LOAD_STORE_PAIR";
        case InstrCategory::COMPARE: return "COMPARE";
        case InstrCategory::MOVE: return "MOVE";
        case InstrCategory::SYSTEM: return "SYSTEM";
        case InstrCategory::SIMD_FP: return "SIMD_FP";
        case InstrCategory::SYSCALL: return "SYSCALL";
        default: return "UNKNOWN";
    }
}

const char* ARM64Decoder::mnemonic(const DecodedInstr& instr) {
    switch (instr.opcode) {
        case 0x00: return "ADD";
        case 0x01: return "SUB";
        case 0x02: return "MUL";
        case 0x03: return "MADD";
        case 0x04: return "UDIV";
        case 0x05: return "SDIV";
        case 0x10: return "AND";
        case 0x11: return "BIC";
        case 0x12: return "ORR";
        case 0x13: return "ORN";
        case 0x14: return "EOR";
        case 0x15: return "EON";
        case 0x16: return "ANDS";
        case 0x17: return "BICS";
        case 0x20: return "MOV";
        case 0x21: return "MOVN";
        case 0x22: return "MOVZ";
        case 0x23: return "MOVK";
        case 0x24: return "ADR";
        case 0x25: return "ADRP";
        case 0x30: return "CMP";
        case 0x31: return "TST";
        case 0x40: return "CSEL";
        case 0x50: return "B";
        case 0x51: return "BL";
        case 0x52: return "BR";
        case 0x53: return "RET";
        case 0x54: return "B.cond";
        case 0x55: return "CBZ";
        case 0x56: return "CBNZ";
        case 0x57: return "TBZ";
        case 0x58: return "TBNZ";
        case 0x60: return "LDRB";
        case 0x61: return "LDRH";
        case 0x62: return "LDR";
        case 0x63: return "LDR";
        case 0x70: return "STRB";
        case 0x71: return "STRH";
        case 0x72: return "STR";
        case 0x73: return "STR";
        case 0x80: return "LDP";
        case 0x81: return "STP";
        case 0x90: return "NOP";
        case 0x91: return "DMB";
        case 0x92: return "DSB";
        case 0x93: return "ISB";
        case 0x94: return "MRS";
        case 0x95: return "MSR";
        default: return "???";
    }
}

} // namespace bbarm64
