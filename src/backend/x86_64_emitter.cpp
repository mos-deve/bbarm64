#include "x86_64_emitter.hpp"
#include <cstring>

namespace bbarm64 {

X86_64Emitter::X86_64Emitter() : buf_(nullptr), size_(0), offset_(0) {}

void X86_64Emitter::set_buffer(uint8_t* buf, size_t size) {
    buf_ = buf;
    size_ = size;
    offset_ = 0;
}

void X86_64Emitter::emit_byte(uint8_t b) {
    if (buf_ && offset_ < size_) buf_[offset_++] = b;
}

void X86_64Emitter::emit_word(uint16_t w) {
    emit_byte(w & 0xFF);
    emit_byte((w >> 8) & 0xFF);
}

void X86_64Emitter::emit_dword(uint32_t d) {
    emit_byte(d & 0xFF);
    emit_byte((d >> 8) & 0xFF);
    emit_byte((d >> 16) & 0xFF);
    emit_byte((d >> 24) & 0xFF);
}

void X86_64Emitter::emit_qword(uint64_t q) {
    emit_dword(q & 0xFFFFFFFF);
    emit_dword(q >> 32);
}

void X86_64Emitter::emit_rex(bool w, uint8_t r, uint8_t x, uint8_t b) {
    emit_byte(0x40 | (w ? 0x08 : 0) | ((r & 1) << 2) | ((x & 1) << 1) | (b & 1));
}

void X86_64Emitter::emit_vex(bool r, bool x, bool b, uint8_t mmmmm, bool w, uint8_t vvvv, uint8_t pp) {
    emit_byte(0xC4);
    emit_byte((r ? 0 : 0x80) | (x ? 0 : 0x40) | (b ? 0 : 0x20) | mmmmm);
    emit_byte((w ? 0x80 : 0) | ((vvvv & 0xF) << 3) | pp);
}

void X86_64Emitter::emit_mov_reg_imm(uint8_t reg, uint64_t imm) {
    if (reg >= 8) emit_rex(true, 0, 0, 1);
    else emit_rex(true, 0, 0, 0);
    emit_byte(0xB8 + (reg & 7));
    emit_qword(imm);
}

void X86_64Emitter::emit_mov_reg_reg(uint8_t dst, uint8_t src) {
    emit_rex(true, (src >> 3) & 1, 0, (dst >> 3) & 1);
    emit_byte(0x89);
    emit_byte(0xC0 | ((src & 7) << 3) | (dst & 7));
}

void X86_64Emitter::emit_add_reg_reg(uint8_t dst, uint8_t src) {
    emit_rex(true, (src >> 3) & 1, 0, (dst >> 3) & 1);
    emit_byte(0x01);
    emit_byte(0xC0 | ((src & 7) << 3) | (dst & 7));
}

void X86_64Emitter::emit_sub_reg_reg(uint8_t dst, uint8_t src) {
    emit_rex(true, (src >> 3) & 1, 0, (dst >> 3) & 1);
    emit_byte(0x29);
    emit_byte(0xC0 | ((src & 7) << 3) | (dst & 7));
}

void X86_64Emitter::emit_and_reg_reg(uint8_t dst, uint8_t src) {
    emit_rex(true, (src >> 3) & 1, 0, (dst >> 3) & 1);
    emit_byte(0x21);
    emit_byte(0xC0 | ((src & 7) << 3) | (dst & 7));
}

void X86_64Emitter::emit_or_reg_reg(uint8_t dst, uint8_t src) {
    emit_rex(true, (src >> 3) & 1, 0, (dst >> 3) & 1);
    emit_byte(0x09);
    emit_byte(0xC0 | ((src & 7) << 3) | (dst & 7));
}

void X86_64Emitter::emit_xor_reg_reg(uint8_t dst, uint8_t src) {
    emit_rex(true, (src >> 3) & 1, 0, (dst >> 3) & 1);
    emit_byte(0x31);
    emit_byte(0xC0 | ((src & 7) << 3) | (dst & 7));
}

void X86_64Emitter::emit_mul_reg(uint8_t src) {
    emit_rex(true, 0, 0, src & 1);
    emit_byte(0xF7);
    emit_byte(0xE0 | (src & 7));
}

void X86_64Emitter::emit_imul_reg_reg(uint8_t dst, uint8_t src) {
    emit_rex(true, (src >> 3) & 1, 0, (dst >> 3) & 1);
    emit_byte(0x0F);
    emit_byte(0xAF);
    emit_byte(0xC0 | ((src & 7) << 3) | (dst & 7));
}

void X86_64Emitter::emit_div_reg(uint8_t src) {
    emit_rex(true, 0, 0, src & 1);
    emit_byte(0xF7);
    emit_byte(0xF0 | (src & 7));
}

void X86_64Emitter::emit_idiv_reg(uint8_t src) {
    emit_rex(true, 0, 0, src & 1);
    emit_byte(0xF7);
    emit_byte(0xF8 | (src & 7));
}

void X86_64Emitter::emit_shl_reg_imm(uint8_t reg, uint8_t imm) {
    emit_rex(true, 0, 0, reg & 1);
    emit_byte(0xC1);
    emit_byte(0xE0 | (reg & 7));
    emit_byte(imm);
}

void X86_64Emitter::emit_shr_reg_imm(uint8_t reg, uint8_t imm) {
    emit_rex(true, 0, 0, reg & 1);
    emit_byte(0xC1);
    emit_byte(0xE8 | (reg & 7));
    emit_byte(imm);
}

void X86_64Emitter::emit_sar_reg_imm(uint8_t reg, uint8_t imm) {
    emit_rex(true, 0, 0, reg & 1);
    emit_byte(0xC1);
    emit_byte(0xF8 | (reg & 7));
    emit_byte(imm);
}

void X86_64Emitter::emit_ror_reg_imm(uint8_t reg, uint8_t imm) {
    emit_rex(true, 0, 0, reg & 1);
    emit_byte(0xC1);
    emit_byte(0xC8 | (reg & 7));
    emit_byte(imm);
}

void X86_64Emitter::emit_not_reg(uint8_t reg) {
    emit_rex(true, 0, 0, reg & 1);
    emit_byte(0xF7);
    emit_byte(0xD0 | (reg & 7));
}

void X86_64Emitter::emit_neg_reg(uint8_t reg) {
    emit_rex(true, 0, 0, reg & 1);
    emit_byte(0xF7);
    emit_byte(0xD8 | (reg & 7));
}

void X86_64Emitter::emit_cmp_reg_reg(uint8_t dst, uint8_t src) {
    emit_rex(true, (src >> 3) & 1, 0, (dst >> 3) & 1);
    emit_byte(0x39);
    emit_byte(0xC0 | ((src & 7) << 3) | (dst & 7));
}

void X86_64Emitter::emit_test_reg_reg(uint8_t dst, uint8_t src) {
    emit_rex(true, (src >> 3) & 1, 0, (dst >> 3) & 1);
    emit_byte(0x85);
    emit_byte(0xC0 | ((src & 7) << 3) | (dst & 7));
}

void X86_64Emitter::emit_test_reg_imm(uint8_t reg, uint32_t imm) {
    emit_rex(true, 0, 0, reg & 1);
    emit_byte(0xF7);
    emit_byte(0xC0 | (reg & 7));
    emit_dword(imm);
}

void X86_64Emitter::emit_mov_reg_mem(uint8_t reg, uint8_t base, int32_t disp) {
    emit_rex(true, (reg >> 3) & 1, 0, (base >> 3) & 1);
    emit_byte(0x8B);
    if (disp == 0 && (base & 7) != 4) {
        emit_byte(((reg & 7) << 3) | (base & 7));
    } else if (disp == 0 && (base & 7) == 4) {
        emit_byte(((reg & 7) << 3) | 0x04);
        emit_byte(0x24);
    } else if (disp >= -128 && disp <= 127 && (base & 7) != 4) {
        emit_byte(0x40 | ((reg & 7) << 3) | (base & 7));
        emit_byte(disp & 0xFF);
    } else if (disp >= -128 && disp <= 127 && (base & 7) == 4) {
        emit_byte(0x40 | ((reg & 7) << 3) | 0x04);
        emit_byte(0x24);
        emit_byte(disp & 0xFF);
    } else if ((base & 7) != 4) {
        emit_byte(0x80 | ((reg & 7) << 3) | (base & 7));
        emit_dword(disp);
    } else {
        emit_byte(0x80 | ((reg & 7) << 3) | 0x04);
        emit_byte(0x24);
        emit_dword(disp);
    }
}

void X86_64Emitter::emit_mov_mem_reg(uint8_t base, int32_t disp, uint8_t reg) {
    emit_rex(true, (reg >> 3) & 1, 0, (base >> 3) & 1);
    emit_byte(0x89);
    if (disp == 0 && (base & 7) != 4) {
        emit_byte(((reg & 7) << 3) | (base & 7));
    } else if (disp == 0 && (base & 7) == 4) {
        emit_byte(((reg & 7) << 3) | 0x04);
        emit_byte(0x24);
    } else if (disp >= -128 && disp <= 127 && (base & 7) != 4) {
        emit_byte(0x40 | ((reg & 7) << 3) | (base & 7));
        emit_byte(disp & 0xFF);
    } else if (disp >= -128 && disp <= 127 && (base & 7) == 4) {
        emit_byte(0x40 | ((reg & 7) << 3) | 0x04);
        emit_byte(0x24);
        emit_byte(disp & 0xFF);
    } else if ((base & 7) != 4) {
        emit_byte(0x80 | ((reg & 7) << 3) | (base & 7));
        emit_dword(disp);
    } else {
        emit_byte(0x80 | ((reg & 7) << 3) | 0x04);
        emit_byte(0x24);
        emit_dword(disp);
    }
}

void X86_64Emitter::emit_mov_reg_mem64(uint8_t reg, uint8_t base, int32_t disp) {
    emit_mov_reg_mem(reg, base, disp);
}

void X86_64Emitter::emit_mov_mem64_reg(uint8_t base, int32_t disp, uint8_t reg) {
    emit_mov_mem_reg(base, disp, reg);
}

void X86_64Emitter::emit_jmp_rel(int32_t offset) {
    emit_byte(0xE9);
    emit_dword(offset - 4);
}

void X86_64Emitter::emit_jcc_rel(uint8_t cc, int32_t offset) {
    emit_byte(0x0F);
    emit_byte(0x80 | cc);
    emit_dword(offset - 4);
}

void X86_64Emitter::emit_call_rel(int32_t offset) {
    emit_byte(0xE8);
    emit_dword(offset - 4);
}

void X86_64Emitter::emit_ret() {
    emit_byte(0xC3);
}

void X86_64Emitter::emit_cmovcc_reg_reg(uint8_t cc, uint8_t dst, uint8_t src) {
    emit_rex(true, (src >> 3) & 1, 0, (dst >> 3) & 1);
    emit_byte(0x0F);
    emit_byte(0x40 | cc);
    emit_byte(0xC0 | ((src & 7) << 3) | (dst & 7));
}

void X86_64Emitter::emit_cdq() {
    emit_byte(0x99);
}

void X86_64Emitter::emit_cqo() {
    emit_rex(true, 0, 0, 0);
    emit_byte(0x99);
}

void X86_64Emitter::emit_bswap(uint8_t reg) {
    emit_rex(true, 0, 0, reg & 1);
    emit_byte(0x0F);
    emit_byte(0xC8 | (reg & 7));
}

void X86_64Emitter::emit_lzcnt(uint8_t dst, uint8_t src) {
    emit_rex(true, (src >> 3) & 1, 0, (dst >> 3) & 1);
    emit_byte(0xF3);
    emit_byte(0x0F);
    emit_byte(0xBD);
    emit_byte(0xC0 | ((src & 7) << 3) | (dst & 7));
}

void X86_64Emitter::emit_tzcnt(uint8_t dst, uint8_t src) {
    emit_rex(true, (src >> 3) & 1, 0, (dst >> 3) & 1);
    emit_byte(0xF3);
    emit_byte(0x0F);
    emit_byte(0xBC);
    emit_byte(0xC0 | ((src & 7) << 3) | (dst & 7));
}

void X86_64Emitter::emit_nop() {
    emit_byte(0x90);
}

void X86_64Emitter::emit_int3() {
    emit_byte(0xCC);
}

void X86_64Emitter::emit_add_reg_imm(uint8_t reg, uint64_t imm) {
    if (imm <= 0xFFFFFFFF) {
        emit_rex(true, 0, 0, reg & 1);
        emit_byte(0x81);
        emit_byte(0xC0 | (reg & 7));
        emit_dword(static_cast<uint32_t>(imm));
    } else {
        emit_mov_reg_reg(RAX, reg);
        emit_rex(true, 0, 0, 0);
        emit_byte(0x48);
        emit_byte(0x05);
        emit_qword(imm);
        emit_mov_reg_reg(reg, RAX);
    }
}

void X86_64Emitter::emit_sub_reg_imm(uint8_t reg, uint64_t imm) {
    if (imm <= 0xFFFFFFFF) {
        emit_rex(true, 0, 0, reg & 1);
        emit_byte(0x81);
        emit_byte(0xE8 | (reg & 7));
        emit_dword(static_cast<uint32_t>(imm));
    } else {
        emit_mov_reg_reg(RAX, reg);
        emit_rex(true, 0, 0, 0);
        emit_byte(0x2D);
        emit_qword(imm);
        emit_mov_reg_reg(reg, RAX);
    }
}

void X86_64Emitter::emit_and_reg_imm(uint8_t reg, uint64_t imm) {
    if (imm <= 0xFFFFFFFF) {
        emit_rex(true, 0, 0, reg & 1);
        emit_byte(0x81);
        emit_byte(0xE0 | (reg & 7));
        emit_dword(static_cast<uint32_t>(imm));
    } else {
        emit_mov_reg_imm(reg, imm);
    }
}

void X86_64Emitter::emit_or_reg_imm(uint8_t reg, uint64_t imm) {
    if (imm <= 0xFFFFFFFF) {
        emit_rex(true, 0, 0, reg & 1);
        emit_byte(0x81);
        emit_byte(0xC8 | (reg & 7));
        emit_dword(static_cast<uint32_t>(imm));
    } else {
        emit_mov_reg_imm(reg, imm);
    }
}

void X86_64Emitter::emit_xor_reg_imm(uint8_t reg, uint64_t imm) {
    if (imm <= 0xFFFFFFFF) {
        emit_rex(true, 0, 0, reg & 1);
        emit_byte(0x81);
        emit_byte(0xF0 | (reg & 7));
        emit_dword(static_cast<uint32_t>(imm));
    } else {
        emit_mov_reg_imm(reg, imm);
    }
}

void X86_64Emitter::emit_cmp_reg_imm(uint8_t reg, uint64_t imm) {
    if (imm <= 0xFFFFFFFF) {
        emit_rex(true, 0, 0, reg & 1);
        emit_byte(0x81);
        emit_byte(0xF8 | (reg & 7));
        emit_dword(static_cast<uint32_t>(imm));
    } else {
        emit_mov_reg_reg(RAX, reg);
        emit_rex(true, 0, 0, 0);
        emit_byte(0x3D);
        emit_qword(imm);
    }
}

} // namespace bbarm64
