#pragma once
#include "cpu_context.hpp"
#include "memory_manager.hpp"
#include "decoder/arm64_decoder.hpp"
#include "cache/translation_cache.hpp"
#include <cstdint>
#include <csignal>
#include <csetjmp>

namespace bbarm64 {

class ExecEngine {
public:
    ExecEngine(CPUContext& ctx, MemoryManager& mem, TranslationCache& cache);

    // Main entry point - start executing from given ARM64 PC
    void run(uint64_t arm_pc);

    // Translate a block and add to cache
    BlockInfo* translate_block(uint64_t arm_pc);

    // Execute a single translated block
    void execute_block(BlockInfo* block);

    // Handle a syscall (called from translated code)
    int handle_syscall(CPUContext& ctx);

    // Signal handler for SMC detection
    static void signal_handler(int sig, siginfo_t* info, void* ucontext);

private:
    // Execute a decoded instruction by directly manipulating CPUContext
    void execute_decoded(const struct DecodedInstr& decoded);
    CPUContext& ctx_;
    MemoryManager& mem_;
    TranslationCache& cache_;
    ARM64Decoder decoder_;
    jmp_buf crash_jmp_buf_;
    
    static ExecEngine* s_instance;
};

} // namespace bbarm64
