#include "exec_engine.hpp"
#include "decoder/arm64_decoder.hpp"
#include "ir/ir.hpp"
#include "backend/x86_64_emitter.hpp"
#include "backend/x86_64_regalloc.hpp"
#include "backend/x86_64_lower.hpp"
#include "syscall/syscall_handlers.hpp"
#include <cstdio>
#include <csignal>
#include <cstring>
#include <cstdlib>
#include <csetjmp>
#include <csetjmp>

namespace bbarm64 {

ExecEngine::ExecEngine(CPUContext& ctx, MemoryManager& mem, TranslationCache& cache)
    : ctx_(ctx), mem_(mem), cache_(cache), decoder_() {
    s_instance = this;
}

static bool check_condition(uint64_t nzcv, uint8_t cond) {
    bool n = (nzcv >> 31) & 1;
    bool z = (nzcv >> 30) & 1;
    bool c = (nzcv >> 29) & 1;
    bool v = (nzcv >> 28) & 1;
    switch (cond) {
        case 0: return z;
        case 1: return !z;
        case 2: return c;
        case 3: return !c;
        case 4: return n;
        case 5: return !n;
        case 6: return v;
        case 7: return !v;
        case 8: return c && !z;
        case 9: return !c || z;
        case 10: return n == v;
        case 11: return n != v;
        case 12: return !z && n == v;
        case 13: return z || n != v;
        case 14: return true;
        case 15: return false;
        default: return false;
    }
}

void ExecEngine::execute_decoded(const DecodedInstr& decoded) {
    switch (decoded.opcode) {
        case 0x22:
            ctx_.x[decoded.rd] = decoded.imm;
            break;
        case 0x23: {
            uint32_t hw_raw = (decoded.raw >> 21) & 0x3;
            ctx_.x[decoded.rd] = (ctx_.x[decoded.rd] & ~(0xFFFFULL << (hw_raw * 16))) | (decoded.imm << (hw_raw * 16));
            break;
        }
        case 0x21: {
            uint32_t hw_raw = (decoded.raw >> 21) & 0x3;
            ctx_.x[decoded.rd] = ~(decoded.imm << (hw_raw * 16));
            break;
        }
        case 0x24:
            ctx_.x[decoded.rd] = ctx_.pc + decoded.imm_s;
            break;
        case 0x25:
            ctx_.x[decoded.rd] = (ctx_.pc & ~0xFFFULL) + (decoded.imm_s << 12);
            break;
        case 0x00:
            ctx_.x[decoded.rd] = ctx_.x[decoded.rn] + decoded.imm;
            break;
        case 0x01:
            ctx_.x[decoded.rd] = ctx_.x[decoded.rn] - decoded.imm;
            break;
        case 0x12:
            ctx_.x[decoded.rd] = ctx_.x[decoded.rn] | ctx_.x[decoded.rm];
            break;
        case 0x10:
            ctx_.x[decoded.rd] = ctx_.x[decoded.rn] & ctx_.x[decoded.rm];
            break;
        case 0x14:
            ctx_.x[decoded.rd] = ctx_.x[decoded.rn] ^ ctx_.x[decoded.rm];
            break;
        case 0x20:
            ctx_.x[decoded.rd] = (decoded.rm == 31) ? ctx_.sp : ctx_.x[decoded.rm];
            break;
        case 0x63:
        case 0x62: {
            uint64_t addr;
            if (decoded.is_pc_relative) {
                addr = ctx_.pc + (decoded.imm_s << 2);
            } else {
                uint64_t base = (decoded.rn == 31) ? ctx_.sp : ctx_.x[decoded.rn];
                uint64_t offset = decoded.imm;
                if (decoded.imm_s != 0 && decoded.imm == 0) {
                    offset = decoded.imm_s;
                } else if (decoded.rm < 31 && decoded.imm == 0 && decoded.imm_s == 0 && decoded.shift_type < 4) {
                    offset = ctx_.x[decoded.rm] << decoded.shift_type;
                }
                addr = base + offset;
            }
            if (decoded.opcode == 0x63) {
                uint64_t val;
                if (mem_.read_u64(addr, val)) { ctx_.x[decoded.rt] = val; }
                else { fprintf(stderr, "[bbarm64] LDR64 failed at 0x%lx\n", addr); ctx_.state_flags |= CPUContext::STATE_CRASHED; }
            } else {
                uint32_t val;
                if (mem_.read_u32(addr, val)) { ctx_.x[decoded.rt] = val; }
                else { fprintf(stderr, "[bbarm64] LDR32 failed at 0x%lx\n", addr); ctx_.state_flags |= CPUContext::STATE_CRASHED; }
            }
            break;
        }
        case 0x73:
        case 0x72: {
            uint64_t base = (decoded.rn == 31) ? ctx_.sp : ctx_.x[decoded.rn];
            uint64_t offset = decoded.imm;
            if (decoded.imm_s != 0 && decoded.imm == 0) {
                offset = decoded.imm_s;
            } else if (decoded.rm < 31 && decoded.imm == 0 && decoded.imm_s == 0) {
                offset = ctx_.x[decoded.rm] << decoded.shift_type;
            }
            uint64_t addr = base + offset;
            if (decoded.opcode == 0x73) mem_.write_u64(addr, ctx_.x[decoded.rt]);
            else mem_.write_u32(addr, static_cast<uint32_t>(ctx_.x[decoded.rt]));
            break;
        }
        case 0x60: {
            uint64_t base = (decoded.rn == 31) ? ctx_.sp : ctx_.x[decoded.rn];
            uint64_t addr = base + decoded.imm;
            uint8_t val;
            if (mem_.read_u8(addr, val)) { ctx_.x[decoded.rt] = val; }
            else { fprintf(stderr, "[bbarm64] LDRB failed at 0x%lx\n", addr); ctx_.state_flags |= CPUContext::STATE_CRASHED; }
            break;
        }
        case 0x61: {
            uint64_t base = (decoded.rn == 31) ? ctx_.sp : ctx_.x[decoded.rn];
            uint64_t addr = base + decoded.imm;
            uint16_t val;
            if (mem_.read_u16(addr, val)) { ctx_.x[decoded.rt] = val; }
            else { fprintf(stderr, "[bbarm64] LDRH failed at 0x%lx\n", addr); ctx_.state_flags |= CPUContext::STATE_CRASHED; }
            break;
        }
        case 0x70: {
            uint64_t base = (decoded.rn == 31) ? ctx_.sp : ctx_.x[decoded.rn];
            uint64_t addr = base + decoded.imm;
            mem_.write_u8(addr, static_cast<uint8_t>(ctx_.x[decoded.rt]));
            break;
        }
        case 0x71: {
            uint64_t base = (decoded.rn == 31) ? ctx_.sp : ctx_.x[decoded.rn];
            uint64_t addr = base + decoded.imm;
            mem_.write_u16(addr, static_cast<uint16_t>(ctx_.x[decoded.rt]));
            break;
        }
        case 0x80: {
            uint64_t base = (decoded.rn == 31) ? ctx_.sp : ctx_.x[decoded.rn];
            uint64_t addr = base + decoded.imm_s;
            uint64_t val1, val2;
            if (mem_.read_u64(addr, val1) && mem_.read_u64(addr + 8, val2)) {
                ctx_.x[decoded.rt] = val1; ctx_.x[decoded.rm] = val2;
            } else { fprintf(stderr, "[bbarm64] LDP failed at 0x%lx\n", addr); ctx_.state_flags |= CPUContext::STATE_CRASHED; }
            break;
        }
        case 0x81: {
            uint64_t base = (decoded.rn == 31) ? ctx_.sp : ctx_.x[decoded.rn];
            uint64_t addr = base + decoded.imm_s;
            mem_.write_u64(addr, ctx_.x[decoded.rt]);
            mem_.write_u64(addr + 8, ctx_.x[decoded.rm]);
            break;
        }
        case 0x50:
            ctx_.pc += decoded.branch_offset;
            return;
        case 0x51:
            ctx_.x[30] = ctx_.pc + 4;
            ctx_.pc += decoded.branch_offset;
            return;
        case 0x52:
            ctx_.pc = (decoded.rn == 31) ? ctx_.sp : ctx_.x[decoded.rn];
            return;
        case 0x53:
            ctx_.pc = (decoded.rn == 31) ? ctx_.x[30] : ctx_.x[decoded.rn];
            return;
        case 0x54:
            if (check_condition(ctx_.nzcv, decoded.cond)) { ctx_.pc += decoded.branch_offset; return; }
            break;
        case 0x55: {
            uint64_t val = (decoded.rn == 31) ? ctx_.sp : ctx_.x[decoded.rn];
            if (decoded.size) { if (val == 0) { ctx_.pc += decoded.branch_offset; return; } }
            else { if ((uint32_t)val == 0) { ctx_.pc += decoded.branch_offset; return; } }
            break;
        }
        case 0x56: {
            uint64_t val = (decoded.rn == 31) ? ctx_.sp : ctx_.x[decoded.rn];
            if (decoded.size) { if (val != 0) { ctx_.pc += decoded.branch_offset; return; } }
            else { if ((uint32_t)val != 0) { ctx_.pc += decoded.branch_offset; return; } }
            break;
        }
        case 0x57:
        case 0x58: {
            uint64_t val = ctx_.x[decoded.rn];
            uint32_t bit = decoded.imm;
            bool bit_set = (val >> bit) & 1;
            bool cond = (decoded.opcode == 0x58);
            if (bit_set == cond) { ctx_.pc += decoded.branch_offset; return; }
            break;
        }
        case 0x30: {
            uint64_t a = ctx_.x[decoded.rn];
            uint64_t b = ctx_.x[decoded.rm];
            uint64_t result = a - b;
            bool n = (result >> 63) & 1;
            bool z = (result == 0);
            bool c = (a >= b);
            bool v = ((int64_t)a - (int64_t)b < 0) != ((int64_t)a < (int64_t)b);
            ctx_.set_nzcv(n, z, c, v);
            break;
        }
        case 0x31: {
            uint64_t result = ctx_.x[decoded.rn] & ctx_.x[decoded.rm];
            bool n = (result >> 63) & 1;
            bool z = (result == 0);
            ctx_.set_nzcv(n, z, 0, 0);
            break;
        }
        case 0x40:
            if (check_condition(ctx_.nzcv, decoded.cond)) ctx_.x[decoded.rd] = ctx_.x[decoded.rn];
            else ctx_.x[decoded.rd] = ctx_.x[decoded.rm];
            break;
        case 0x90:
            break;
        default:
            fprintf(stderr, "[bbarm64] Unknown opcode 0x%02x at 0x%lx: 0x%08x\n", decoded.opcode, ctx_.pc, decoded.raw);
            ctx_.state_flags |= CPUContext::STATE_CRASHED;
            break;
    }
    ctx_.pc += 4;
}

void ExecEngine::run(uint64_t arm_pc) {
    ctx_.pc = arm_pc;
    fprintf(stderr, "[bbarm64] Starting execution at 0x%lx (JIT mode)\n", arm_pc);
    int max_iter = 10000000;
    int iter = 0;
    while (!(ctx_.state_flags & CPUContext::STATE_CRASHED) &&
           !(ctx_.state_flags & CPUContext::STATE_EXITED) &&
           iter < max_iter) {
        BlockInfo* block = cache_.lookup(ctx_.pc);
        if (block) {
            execute_block(block);
            iter++;
            continue;
        }
        uint32_t instr;
        if (!mem_.read_u32(ctx_.pc, instr)) {
            fprintf(stderr, "[bbarm64] Failed to read instruction at 0x%lx\n", ctx_.pc);
            break;
        }
        if ((instr & 0xFFE0001F) == 0xD4000001) {
            fprintf(stderr, "[bbarm64] SVC at 0x%lx, x8=%lu\n", ctx_.pc, ctx_.x[8]);
            ctx_.pc += 4;
            int64_t result = handle_syscall(ctx_);
            ctx_.x[0] = static_cast<uint64_t>(result);
            iter++;
            continue;
        }
        block = translate_block(ctx_.pc);
        if (block) {
            fprintf(stderr, "[bbarm64] Translated block at 0x%lx, size=%u, instrs=%u\n",
                    ctx_.pc, block->host_code_size, block->arm_instr_count);
            execute_block(block);
            fprintf(stderr, "[bbarm64] After execute_block, pc=0x%lx\n", ctx_.pc);
            iter++;
        } else {
            fprintf(stderr, "[bbarm64] translate_block returned nullptr at 0x%lx, falling back to interpreter\n", ctx_.pc);
            DecodedInstr decoded;
            if (!decoder_.decode(instr, decoded)) {
                fprintf(stderr, "[bbarm64] Unknown instr 0x%08x at 0x%lx\n", instr, ctx_.pc);
                ctx_.state_flags |= CPUContext::STATE_CRASHED;
                continue;
            }
            execute_decoded(decoded);
            iter++;
        }
    }
    if (iter >= max_iter) fprintf(stderr, "[bbarm64] Hit iteration limit at pc=0x%lx\n", ctx_.pc);
    if (ctx_.state_flags & CPUContext::STATE_EXITED)
        fprintf(stderr, "[bbarm64] Program exited with status %d\n", ctx_.exit_status);
}

BlockInfo* ExecEngine::translate_block(uint64_t arm_pc) {
    IRBuilder builder;
    IROptimizer optimizer;
    X86_64Emitter emitter;
    X86_64RegAlloc regalloc;
    std::vector<DecodedInstr> instructions;
    uint64_t pc = arm_pc;
    const int max_instr = 64;
    for (int i = 0; i < max_instr; i++) {
        uint32_t raw;
        if (!mem_.read_u32(pc, raw)) break;
        if ((raw & 0xFFE0001F) == 0xD4000001) {
            break;
        }
        DecodedInstr instr;
        if (!decoder_.decode(raw, instr)) {
            fprintf(stderr, "[bbarm64] Failed to decode at 0x%lx: 0x%08x\n", pc, raw);
            break;
        }
        instructions.push_back(instr);
        // Stop BEFORE branches — let the interpreter handle them
        if (instr.category == InstrCategory::BRANCH ||
            instr.category == InstrCategory::BRANCH_COND) break;
        pc += 4;
    }
    if (instructions.empty()) return nullptr;
    IRBlock ir_block;
    if (!builder.build(instructions.data(), instructions.size(), arm_pc, ir_block)) return nullptr;
    optimizer.optimize(ir_block);
    uint8_t* host_code = cache_.alloc_host_code(4096);
    if (!host_code) return nullptr;
    emitter.set_buffer(host_code, 4096);
    IRLowering lowering(emitter, regalloc);
    if (!lowering.lower_block(ir_block, ctx_, arm_pc + instructions.size() * 4)) return nullptr;
    BlockInfo* block = new BlockInfo();
    block->arm_pc = arm_pc;
    block->host_code = host_code;
    block->host_code_size = emitter.code_size();
    block->arm_instr_count = ir_block.arm_instr_count;
    cache_.insert(arm_pc, host_code, emitter.code_size(), ir_block.arm_instr_count);
    mem_.mark_translated_page(arm_pc);
    return block;
}

void ExecEngine::execute_block(BlockInfo* block) {
    if (!block || !block->host_code || !block->is_valid) return;
    __builtin___clear_cache(reinterpret_cast<char*>(block->host_code),
                            reinterpret_cast<char*>(block->host_code + block->host_code_size));
    fprintf(stderr, "[bbarm64] Executing block at host %p, size=%u, ctx.pc=0x%lx, ctx.sp=0x%lx\n",
            (void*)block->host_code, block->host_code_size, ctx_.pc, ctx_.sp);
    // Dump generated code
    fprintf(stderr, "[bbarm64] JIT code (%zu bytes): ", block->host_code_size);
    for (size_t i = 0; i < block->host_code_size && i < 512; i++) {
        fprintf(stderr, "%02x ", block->host_code[i]);
    }
    fprintf(stderr, "\n");
    if (setjmp(crash_jmp_buf_) != 0) {
        fprintf(stderr, "[bbarm64] JIT code crashed during execution\n");
        return;
    }
    using BlockFunc = void (*)(CPUContext*);
    BlockFunc func = reinterpret_cast<BlockFunc>(block->host_code);
    func(&ctx_);
    fprintf(stderr, "[bbarm64] Block returned, pc=0x%lx, sp=0x%lx, x0=0x%lx, x5=0x%lx\n",
            ctx_.pc, ctx_.sp, ctx_.x[0], ctx_.x[5]);
}

int ExecEngine::handle_syscall(CPUContext& ctx) {
    SyscallHandler handler(mem_);
    return static_cast<int>(handler.execute(ctx));
}

void ExecEngine::signal_handler(int sig, siginfo_t* info, void* ucontext) {
    (void)sig; (void)ucontext;
    if (s_instance && !(s_instance->ctx_.state_flags & CPUContext::STATE_SINGLE_STEP)) {
        fprintf(stderr, "\n[bbarm64] FATAL: Segmentation fault at host address %p\n", info->si_addr);
        fprintf(stderr, "[bbarm64] Current guest PC: 0x%016lx\n", s_instance ? s_instance->ctx_.pc : 0);
    }
    if (s_instance) s_instance->ctx_.state_flags |= CPUContext::STATE_CRASHED;
    // Don't call _exit - let the run loop handle it
    longjmp(s_instance->crash_jmp_buf_, 1);
}

ExecEngine* ExecEngine::s_instance = nullptr;

} // namespace bbarm64
