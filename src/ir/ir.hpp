#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <array>

namespace bbarm64 {

enum class IROpcode : uint8_t {
    ADD, SUB, MUL, UDIV, SDIV, NEG,
    AND, OR, XOR, NOT, SHL, SHR, SAR, ROR,
    ZEXT8, ZEXT16, ZEXT32,
    SEXT8, SEXT16, SEXT32,
    CMP, TST,
    CMOV_EQ, CMOV_NE, CMOV_LT, CMOV_LE,
    CMOV_GT, CMOV_GE, CMOV_LO, CMOV_LS,
    CMOV_HS, CMOV_HI, CMOV_MI, CMOV_PL,
    CMOV_VS, CMOV_VC, CMOV_CS, CMOV_CC,
    LOAD8, LOAD16, LOAD32, LOAD64,
    STORE8, STORE16, STORE32, STORE64,
    LOAD_PAIR, STORE_PAIR,
    JMP, JMP_COND, CALL, RET, SYSCALL,
    GET_REG, SET_REG, MOV_IMM,
    NOP, BSWAP16, BSWAP32, BSWAP64,
    CLZ, CTZ,
    FADD, FSUB, FMUL, FDIV, FCMP,
    VEC_ADD8, VEC_ADD16, VEC_ADD32, VEC_ADD64,
    VEC_SUB8, VEC_SUB16, VEC_SUB32, VEC_SUB64,
    VEC_AND, VEC_OR, VEC_XOR,
    VEC_DUP,
    BARRIER,
};

using IRReg = uint16_t;
static constexpr IRReg IR_NONE = 0;
static constexpr IRReg IR_IMM = 0xFFFF;

struct IRInst {
    IROpcode op;
    IRReg dest;
    IRReg src1;
    IRReg src2;
    uint64_t imm;
    uint64_t imm2;
    uint8_t size;
    uint8_t cond;
    bool flags_set;
    bool flags_used;

    IRInst() : op(IROpcode::NOP), dest(IR_NONE), src1(IR_NONE),
               src2(IR_NONE), imm(0), imm2(0), size(8), cond(0),
               flags_set(false), flags_used(false) {}
};

struct IRBlock {
    std::vector<IRInst> instructions;
    uint64_t arm_pc_start;
    uint64_t arm_pc_end;
    uint32_t arm_instr_count;
    bool is_conditional;
    uint64_t branch_target_taken;
    uint64_t branch_target_fallthrough;

    IRBlock() : arm_pc_start(0), arm_pc_end(0), arm_instr_count(0),
                is_conditional(false), branch_target_taken(0),
                branch_target_fallthrough(0) {}

    IRReg alloc_reg() {
        static IRReg next_reg = 1;
        return next_reg++;
    }

    void reset_reg_counter() {
    }
};

class IRBuilder {
public:
    IRBuilder();
    bool build(const struct DecodedInstr* instructions, uint32_t count,
               uint64_t start_pc, IRBlock& out);
private:
    IRReg next_reg_;
    IRReg alloc_reg(IRBlock& block);
    bool build_instr(const struct DecodedInstr& instr, uint64_t pc, IRBlock& block);
};

class IROptimizer {
public:
    IROptimizer();
    void optimize(IRBlock& block);
private:
    void constant_propagation(IRBlock& block);
    void dead_code_elimination(IRBlock& block);
    void flag_merging(IRBlock& block);
};

} // namespace bbarm64
