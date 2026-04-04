#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

namespace bbarm64 {

// x86_64 register encoding
static constexpr uint8_t RAX = 0, RCX = 1, RDX = 2, RBX = 3;
static constexpr uint8_t RSP = 4, RBP = 5, RSI = 6, RDI = 7;
static constexpr uint8_t R8 = 8, R9 = 9, R10 = 10, R11 = 11;
static constexpr uint8_t R12 = 12, R13 = 13, R14 = 14, R15 = 15;

// Condition codes
static constexpr uint8_t CC_O = 0, CC_NO = 1, CC_B = 2, CC_NB = 3;
static constexpr uint8_t CC_Z = 4, CC_NZ = 5, CC_BE = 6, CC_NBE = 7;
static constexpr uint8_t CC_S = 8, CC_NS = 9, CC_P = 10, CC_NP = 11;
static constexpr uint8_t CC_L = 12, CC_NL = 13, CC_LE = 14, CC_NLE = 15;

// x86_64 code emitter - raw byte emission
class X86_64Emitter {
public:
    X86_64Emitter();

    // Set output buffer
    void set_buffer(uint8_t* buf, size_t size);

    // Get current position
    size_t offset() const { return offset_; }
    uint8_t* current() { return buf_ ? buf_ + offset_ : nullptr; }

    // Emit raw bytes
    void emit_byte(uint8_t b);
    void emit_word(uint16_t w);
    void emit_dword(uint32_t d);
    void emit_qword(uint64_t q);

    // Emit x86_64 instructions
    void emit_mov_reg_imm(uint8_t reg, uint64_t imm);
    void emit_mov_reg_reg(uint8_t dst, uint8_t src);
    void emit_add_reg_reg(uint8_t dst, uint8_t src);
    void emit_sub_reg_reg(uint8_t dst, uint8_t src);
    void emit_and_reg_reg(uint8_t dst, uint8_t src);
    void emit_or_reg_reg(uint8_t dst, uint8_t src);
    void emit_xor_reg_reg(uint8_t dst, uint8_t src);
    void emit_mul_reg(uint8_t src);
    void emit_imul_reg_reg(uint8_t dst, uint8_t src);
    void emit_div_reg(uint8_t src);
    void emit_idiv_reg(uint8_t src);
    void emit_shl_reg_imm(uint8_t reg, uint8_t imm);
    void emit_shr_reg_imm(uint8_t reg, uint8_t imm);
    void emit_sar_reg_imm(uint8_t reg, uint8_t imm);
    void emit_ror_reg_imm(uint8_t reg, uint8_t imm);
    void emit_not_reg(uint8_t reg);
    void emit_neg_reg(uint8_t reg);
    void emit_cmp_reg_reg(uint8_t dst, uint8_t src);
    void emit_test_reg_reg(uint8_t dst, uint8_t src);
    void emit_test_reg_imm(uint8_t reg, uint32_t imm);

    // Memory operations
    void emit_mov_reg_mem(uint8_t reg, uint8_t base, int32_t disp);
    void emit_mov_mem_reg(uint8_t base, int32_t disp, uint8_t reg);
    void emit_mov_reg_mem64(uint8_t reg, uint8_t base, int32_t disp);
    void emit_mov_mem64_reg(uint8_t base, int32_t disp, uint8_t reg);

    // Arithmetic with immediates
    void emit_add_reg_imm(uint8_t reg, uint64_t imm);
    void emit_sub_reg_imm(uint8_t reg, uint64_t imm);
    void emit_and_reg_imm(uint8_t reg, uint64_t imm);
    void emit_or_reg_imm(uint8_t reg, uint64_t imm);
    void emit_xor_reg_imm(uint8_t reg, uint64_t imm);
    void emit_cmp_reg_imm(uint8_t reg, uint64_t imm);

    // Control flow
    void emit_jmp_rel(int32_t offset);
    void emit_jcc_rel(uint8_t cc, int32_t offset);
    void emit_call_rel(int32_t offset);
    void emit_ret();

    // Conditional move
    void emit_cmovcc_reg_reg(uint8_t cc, uint8_t dst, uint8_t src);

    // Extensions
    void emit_movzx_reg8_reg(uint8_t dst, uint8_t src);
    void emit_movzx_reg16_reg(uint8_t dst, uint8_t src);
    void emit_movsx_reg8_reg(uint8_t dst, uint8_t src);
    void emit_movsx_reg16_reg(uint8_t dst, uint8_t src);
    void emit_cdq();
    void emit_cqo();

    // BSWAP
    void emit_bswap(uint8_t reg);

    // Bit scan
    void emit_lzcnt(uint8_t dst, uint8_t src);
    void emit_tzcnt(uint8_t dst, uint8_t src);

    // NOP (multi-byte)
    void emit_nop();

    // INT3 (debug breakpoint)
    void emit_int3();

    // REX prefix
    void emit_rex(bool w, uint8_t r, uint8_t x, uint8_t b);

    // VEX prefix for AVX2
    void emit_vex(bool r, bool x, bool b, uint8_t mmmmm, bool w, uint8_t vvvv, uint8_t pp);

    // Get emitted code size
    size_t code_size() const { return offset_; }

private:
    uint8_t* buf_;
    size_t size_;
    size_t offset_;
};

} // namespace bbarm64
