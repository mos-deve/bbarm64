#include "syscall_handlers.hpp"
#include "syscall_table.hpp"
#include "../core/memory_manager.hpp"
#include <sys/syscall.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>

namespace bbarm64 {

SyscallHandler::SyscallHandler(MemoryManager& mem) : mem_(mem) {}

int64_t SyscallHandler::execute(CPUContext& ctx) {
    int arm_nr = static_cast<int>(ctx.x[8]);
    int x86_nr = translate_syscall_nr(arm_nr);

    if (x86_nr < 0) {
        fprintf(stderr, "[bbarm64] Unknown syscall: %d\n", arm_nr);
        return -ENOSYS;
    }

    // Route to custom handler if needed
    switch (arm_nr) {
        case 222: return handle_mmap(ctx);
        case 226: return handle_mprotect(ctx);
        case 215: return handle_munmap(ctx);
        case 214: return handle_brk(ctx);
        case 220: return handle_clone(ctx);
        case 98:  return handle_futex(ctx);
        case 93:  return handle_exit(ctx);
        case 94:  return handle_exit_group(ctx);
        case 96:  return handle_set_tid_address(ctx);
        case 100: return handle_set_robust_list(ctx);
        case 178: return handle_gettid(ctx);
        case 167: return handle_prctl(ctx);
        case 139: return handle_rt_sigreturn(ctx);
        case 278: return handle_getrandom(ctx);
        default:  break;
    }

    // Pass-through to host syscall
    return passthrough_syscall(x86_nr, ctx.x[0], ctx.x[1], ctx.x[2],
                                ctx.x[3], ctx.x[4], ctx.x[5]);
}

int64_t SyscallHandler::handle_mmap(CPUContext& ctx) {
    uint64_t addr = ctx.x[0];
    size_t len = ctx.x[1];
    int prot = static_cast<int>(ctx.x[2]);
    int flags = static_cast<int>(ctx.x[3]);
    int fd = static_cast<int>(ctx.x[4]);
    int64_t off = static_cast<int64_t>(ctx.x[5]);

    uint64_t result = mem_.mmap(len, prot, addr);
    if (!result) return -ENOMEM;
    return result;
}

int64_t SyscallHandler::handle_mprotect(CPUContext& ctx) {
    uint64_t addr = ctx.x[0];
    size_t len = ctx.x[1];
    int prot = static_cast<int>(ctx.x[2]);
    return mem_.mprotect(addr, len, prot);
}

int64_t SyscallHandler::handle_munmap(CPUContext& ctx) {
    uint64_t addr = ctx.x[0];
    size_t len = ctx.x[1];
    return mem_.munmap(addr, len);
}

int64_t SyscallHandler::handle_brk(CPUContext& ctx) {
    uint64_t new_brk = ctx.x[0];
    if (new_brk == 0) {
        return mem_.get_brk();
    }
    return mem_.set_brk(new_brk);
}

int64_t SyscallHandler::handle_clone(CPUContext& ctx) {
    // ARM64 clone: X0=flags, X1=stack, X2=ptid, X3=ctid, X4=tls
    // x86_64 clone: RDI=flags, RSI=stack, RDX=ptid, R10=tls, R8=ctid
    unsigned long flags = ctx.x[0];
    void* stack = reinterpret_cast<void*>(ctx.x[1]);
    int* ptid = reinterpret_cast<int*>(ctx.x[2]);
    int* ctid = reinterpret_cast<int*>(ctx.x[3]);
    void* tls = reinterpret_cast<void*>(ctx.x[4]);

    // Use raw clone syscall with x86_64 argument order
    return static_cast<int64_t>(
        syscall(SYS_clone, flags, stack, ptid, tls, ctid)
    );
}

int64_t SyscallHandler::handle_futex(CPUContext& ctx) {
    // Futex is ABI-compatible - direct pass-through
    int* uaddr = reinterpret_cast<int*>(ctx.x[0]);
    int futex_op = static_cast<int>(ctx.x[1]);
    int val = static_cast<int>(ctx.x[2]);
    struct timespec* timeout = reinterpret_cast<struct timespec*>(ctx.x[3]);
    int* uaddr2 = reinterpret_cast<int*>(ctx.x[4]);
    int val3 = static_cast<int>(ctx.x[5]);

    return static_cast<int64_t>(
        syscall(SYS_futex, uaddr, futex_op, val, timeout, uaddr2, val3)
    );
}

int64_t SyscallHandler::handle_exit(CPUContext& ctx) {
    int status = static_cast<int>(ctx.x[0]);
    _exit(status);
    return 0;  // Never reached
}

int64_t SyscallHandler::handle_exit_group(CPUContext& ctx) {
    int status = static_cast<int>(ctx.x[0]);
    _exit(status);
    return 0;
}

int64_t SyscallHandler::handle_set_tid_address(CPUContext& ctx) {
    int* tidptr = reinterpret_cast<int*>(ctx.x[0]);
    return static_cast<int64_t>(
        syscall(SYS_set_tid_address, tidptr)
    );
}

int64_t SyscallHandler::handle_set_robust_list(CPUContext& ctx) {
    void* head = reinterpret_cast<void*>(ctx.x[0]);
    size_t len = ctx.x[1];
    return static_cast<int64_t>(
        syscall(SYS_set_robust_list, head, len)
    );
}

int64_t SyscallHandler::handle_gettid(CPUContext& ctx) {
    (void)ctx;
    return static_cast<int64_t>(gettid());
}

int64_t SyscallHandler::handle_prctl(CPUContext& ctx) {
    int option = static_cast<int>(ctx.x[0]);
    unsigned long arg2 = ctx.x[1];
    unsigned long arg3 = ctx.x[2];
    unsigned long arg4 = ctx.x[3];
    unsigned long arg5 = ctx.x[4];
    return static_cast<int64_t>(
        syscall(SYS_prctl, option, arg2, arg3, arg4, arg5)
    );
}

int64_t SyscallHandler::handle_rt_sigreturn(CPUContext& ctx) {
    (void)ctx;
    // This is handled by the signal frame - just pass through
    return static_cast<int64_t>(
        syscall(SYS_rt_sigreturn)
    );
}

int64_t SyscallHandler::handle_getrandom(CPUContext& ctx) {
    void* buf = reinterpret_cast<void*>(ctx.x[0]);
    size_t buflen = ctx.x[1];
    unsigned int flags = static_cast<unsigned int>(ctx.x[2]);
    return static_cast<int64_t>(
        syscall(SYS_getrandom, buf, buflen, flags)
    );
}

int64_t SyscallHandler::passthrough_syscall(int x86_nr, uint64_t arg0, uint64_t arg1,
                                             uint64_t arg2, uint64_t arg3,
                                             uint64_t arg4, uint64_t arg5) {
    return static_cast<int64_t>(
        syscall(x86_nr, arg0, arg1, arg2, arg3, arg4, arg5)
    );
}

} // namespace bbarm64
