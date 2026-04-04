#include "core/cpu_context.hpp"
#include "core/memory_manager.hpp"
#include "core/exec_engine.hpp"
#include "cache/translation_cache.hpp"
#include "decoder/arm64_decoder.hpp"
#include "elf/elf_loader.hpp"
#include "elf/dynlink.hpp"
#include "syscall/syscall_handlers.hpp"
#include "threading/thread_manager.hpp"
#include "threading/tls_emu.hpp"
#include "util/log.hpp"
#include "util/config.hpp"
#include "util/profiler.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <sys/mman.h>
#include <elf.h>

using namespace bbarm64;

static void print_usage(const char* prog) {
    fprintf(stderr, "bbarm64 v1.2 - ARM64 to x86_64 Dynamic Binary Translator\n");
    fprintf(stderr, "Developed by FAE (Fast Arm Emulation) group\n");
    fprintf(stderr, "Usage: %s <arm64_binary> [args...]\n", prog);
    fprintf(stderr, "\nEnvironment variables:\n");
    fprintf(stderr, "  BBARM64_DUMP=1           Dump translated blocks\n");
    fprintf(stderr, "  BBARM64_LOG_SYSCALLS=1   Log all syscalls\n");
    fprintf(stderr, "  BBARM64_LOG_SIGNALS=1    Log all signals\n");
    fprintf(stderr, "  BBARM64_SMC_DETECT=0     Disable SMC detection\n");
    fprintf(stderr, "  BBARM64_MAX_BLOCK=N      Max instructions per block (default: 64)\n");
    fprintf(stderr, "  BBARM64_CACHE_SIZE=N     FAE-Cache size in MB (default: 64)\n");
    fprintf(stderr, "  BBARM64_AOT_DIR=path     AOT cache directory\n");
    fprintf(stderr, "  BBARM64_MODE=jit|aot|hybrid  Execution mode (default: jit)\n");
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    // Load configuration
    Config config = Config::load();

    // Set up signal handler for SMC detection and syscall interception
    struct sigaction sa;
    sa.sa_sigaction = ExecEngine::signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGTRAP, &sa, nullptr);

    LOG_INFO("bbarm64 v1.1 starting...");
    LOG_INFO("Loading: %s", argv[1]);

    // Initialize subsystems
    CPUContext ctx;
    MemoryManager mem;
    TranslationCache cache(config.cache_size_mb * 1024 * 1024 / 64);  // Estimate entries

    if (!mem.init()) {
        LOG_ERROR("Failed to initialize memory manager");
        return 1;
    }

    if (!cache.init()) {
        LOG_ERROR("Failed to initialize translation cache");
        return 1;
    }

    // Load ELF binary
    ELFLoader loader(mem);
    ELFInfo elf_info;

    if (!loader.load(std::string(argv[1]), elf_info)) {
        LOG_ERROR("Failed to load ELF: %s", argv[1]);
        return 1;
    }

    LOG_INFO("ELF entry point: 0x%lx", elf_info.entry_point);
    LOG_INFO("ELF is dynamic: %s", elf_info.is_dynamic ? "yes" : "no");
    LOG_INFO("ELF base addr: 0x%lx", elf_info.base_addr);
    fprintf(stderr, "[bbarm64] Entry: 0x%lx, base: 0x%lx, dynamic: %d\n",
            elf_info.entry_point, elf_info.base_addr, elf_info.is_dynamic);

    // Load dependent libraries if dynamic
    uint64_t tls_block_addr = 0;
    if (elf_info.is_dynamic) {
        DynLink dynlink(mem);
        LOG_INFO("Dynamic binary - fake ld.so active");

        // Allocate TLS block for dynamic binaries
        tls_block_addr = TLSEmu::allocate_tls_block(4096, mem);
        if (tls_block_addr) {
            ctx.tpidr_el0 = tls_block_addr;
            LOG_INFO("TLS block allocated at 0x%lx", tls_block_addr);
        }
    }

    // Set up initial CPU context
    ctx.pc = elf_info.entry_point;

    // Set up stack: allocate 2MB, push auxv/envp/argv/argc
    uint64_t stack_size = 2 * 1024 * 1024;
    uint64_t stack_base = mem.mmap(stack_size, PROT_READ | PROT_WRITE, 0);
    if (!stack_base) {
        LOG_ERROR("Failed to allocate stack");
        return 1;
    }
    uint64_t sp = stack_base + stack_size;
    sp &= ~0xFULL; // 16-byte align

    // ARM64 stack layout (from high to low addresses):
    // [sp] = argc
    // [sp+8] = argv[0]
    // [sp+16] = NULL (end of argv)
    // [sp+24] = NULL (end of envp)
    // [sp+32] = auxv[0].type
    // [sp+40] = auxv[0].val
    // ...
    // [sp+N] = 0 (AT_NULL type)
    // [sp+N+8] = 0 (AT_NULL val)

    // First, push auxv entries (from high to low addresses)
    // AT_NULL terminator
    sp -= 16;
    mem.write_u64(sp, 0);
    mem.write_u64(sp + 8, 0);

    // AT_RANDOM
    sp -= 16;
    mem.write_u64(sp, 25); // AT_RANDOM
    uint64_t random_addr = sp - 16;
    mem.write_u64(sp + 8, random_addr);
    sp -= 16;
    mem.write_u64(sp, 0);
    mem.write_u64(sp + 8, 0);

    // AT_PHNUM
    sp -= 16;
    mem.write_u64(sp, 5);
    mem.write_u64(sp + 8, elf_info.phnum);

    // AT_PHDR
    sp -= 16;
    mem.write_u64(sp, 3);
    mem.write_u64(sp + 8, elf_info.base_addr + elf_info.phdr_offset);

    // AT_ENTRY
    sp -= 16;
    mem.write_u64(sp, 9);
    mem.write_u64(sp + 8, elf_info.entry_point);

    // AT_HWCAP
    sp -= 16;
    mem.write_u64(sp, 16);
    mem.write_u64(sp + 8, 0x0000000000000FFFULL);

    // AT_PAGESZ
    sp -= 16;
    mem.write_u64(sp, 6);
    mem.write_u64(sp + 8, 4096);

    // AT_SECURE
    sp -= 16;
    mem.write_u64(sp, 23);
    mem.write_u64(sp + 8, 0);

    // AT_EXECFN
    sp -= 16;
    mem.write_u64(sp, 31);
    uint64_t execfn_addr = sp - 64;
    const char* execfn = argv[1];
    size_t execfn_len = strlen(execfn) + 1;
    if (execfn_len > 64) execfn_len = 64;
    for (size_t i = 0; i < execfn_len; i++) {
        mem.write_u8(execfn_addr + i, static_cast<uint8_t>(execfn[i]));
    }
    mem.write_u64(sp + 8, execfn_addr);

    // AT_TLS (for dynamic binaries)
    if (tls_block_addr) {
        sp -= 16;
        mem.write_u64(sp, 23);
        mem.write_u64(sp + 8, tls_block_addr);
    }

    // Save auxv pointer (current sp points to first auxv entry)
    uint64_t auxv_start = sp;

    // envp: NULL terminator
    sp -= 8;
    mem.write_u64(sp, 0);

    // argv: NULL terminator
    sp -= 8;
    mem.write_u64(sp, 0);

    // argv[0] string and pointer
    uint64_t argv0_addr = sp - 64;
    const char* argv0 = argv[1];
    size_t argv0_len = strlen(argv0) + 1;
    if (argv0_len > 64) argv0_len = 64;
    for (size_t i = 0; i < argv0_len; i++) {
        mem.write_u8(argv0_addr + i, static_cast<uint8_t>(argv0[i]));
    }
    sp -= 8;
    mem.write_u64(sp, argv0_addr);

    // argc = 1
    sp -= 8;
    mem.write_u64(sp, 1);

    // Align stack to 16 bytes
    sp &= ~0xFULL;

    ctx.sp = sp;
    ctx.lr = 0;

    // Set x0 to auxv pointer (ARM64 ABI: x0 = auxv at entry)
    ctx.x[0] = auxv_start;

    // Set up TLS for the main thread
    if (tls_block_addr) {
        TLSEmu tls;
        tls.set_tls_base(tls_block_addr);
    }

    // Create execution engine
    ExecEngine engine(ctx, mem, cache);

    LOG_INFO("Starting execution at 0x%lx", ctx.pc);

    // Run!
    engine.run(ctx.pc);

    // Print profiling stats
    Profiler::instance().print_stats();

    LOG_INFO("Execution complete");
    return 0;
}
