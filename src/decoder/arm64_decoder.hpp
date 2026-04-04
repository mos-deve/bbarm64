#pragma once
#include <cstdint>

namespace bbarm64 {

// ARM64 instruction categories
enum class InstrCategory {
    UNKNOWN,
    INTEGER_ARITH,    // ADD, SUB, MUL, etc.
    LOGICAL,          // AND, ORR, EOR, etc.
    SHIFT,            // LSL, LSR, ASR, ROR
    BRANCH,           // B, BL, BR, RET
    BRANCH_COND,      // B.cond, CBZ, CBNZ, TBZ, TBNZ
    LOAD_STORE,       // LDR, STR, LDRB, STRB, etc.
    LOAD_STORE_PAIR,  // LDP, STP
    LOAD_EXCLUSIVE,   // LDXR, STXR
    COMPARE,          // CMP, CMN, TST
    MOVE,             // MOV, MOVN, MOVZ, MOVK
    EXTEND,           // SXTB, SXTH, SXTW, UXTB, UXTH, UXTW
    BITFIELD,         // BFI, BFXIL, UBFM, SBFM
    SYSTEM,           // MRS, MSR, HINT, NOP, DMB, DSB, ISB
    SIMD_FP,          // Floating point and SIMD
    SYSCALL,          // SVC
};

// Decoded ARM64 instruction
struct DecodedInstr {
    uint32_t raw;              // Raw 32-bit instruction word
    InstrCategory category;
    uint8_t opcode;            // Primary opcode
    uint8_t rd;                // Destination register
    uint8_t rn;                // Source register 1
    uint8_t rm;                // Source register 2
    uint8_t rt;                // Transfer register (load/store)
    uint64_t imm;              // Immediate value
    int64_t imm_s;             // Signed immediate
    uint8_t shift_type;        // 0=LSL, 1=LSR, 2=ASR, 3=ROR
    uint8_t shift_amount;
    uint8_t size;              // Operand size: 0=32-bit, 1=64-bit
    uint8_t cond;              // Condition code (for conditional branches)
    bool sets_flags;           // Does this instruction set NZCV?
    bool is_pc_relative;       // Is this a PC-relative load?
    uint32_t branch_offset;    // Branch offset (for B, BL)
    uint8_t q;                 // SIMD Q bit (0=64-bit, 1=128-bit)
    uint8_t vd;                // SIMD destination vector register
};

// ARM64 decoder
class ARM64Decoder {
public:
    ARM64Decoder();

    // Decode a single ARM64 instruction
    bool decode(uint32_t instr, DecodedInstr& out);

    // Get instruction category name (for debugging)
    static const char* category_name(InstrCategory cat);

    // Get instruction mnemonic (for debugging)
    static const char* mnemonic(const DecodedInstr& instr);

private:
    // Decode helpers
    uint32_t bits(uint32_t val, int hi, int lo) const;
    int32_t sign_extend(uint64_t val, int width) const;

    // Category-specific decoders
    bool decode_data_proc_imm(uint32_t instr, DecodedInstr& out);
    bool decode_data_proc_reg(uint32_t instr, DecodedInstr& out);
    bool decode_data_proc_simd(uint32_t instr, DecodedInstr& out);
    bool decode_branch(uint32_t instr, DecodedInstr& out);
    bool decode_load_store(uint32_t instr, DecodedInstr& out);
    bool decode_system(uint32_t instr, DecodedInstr& out);
    bool decode_pcrel(uint32_t instr, DecodedInstr& out);
    bool decode_test_branch(uint32_t instr, DecodedInstr& out);
    bool decode_simd(uint32_t instr, DecodedInstr& out);
};

} // namespace bbarm64
