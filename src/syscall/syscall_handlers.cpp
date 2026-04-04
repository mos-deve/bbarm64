#include "syscall_handlers.hpp"
#include "syscall_table.hpp"
#include "../core/memory_manager.hpp"
#include <sys/syscall.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>

namespace bbarm64 {

SyscallHandler::SyscallHandler(MemoryManager& mem) : mem_(mem) {}

int64_t SyscallHandler::execute(CPUContext& ctx) {
    int arm_nr = static_cast<int>(ctx.x[8]);
    int x86_nr = translate_syscall_nr(arm_nr);
    static bool verbose_logging = getenv("BBARM64_VERBOSE") != nullptr;

    if (x86_nr < 0) {
        fprintf(stderr, "[bbarm64] CRITICAL: Unknown syscall %d (%s)\n", arm_nr, get_syscall_name(arm_nr));
        return -ENOSYS;
    }

    if (verbose_logging) {
        fprintf(stderr, "[bbarm64] Syscall: %-4d %-18s x86: %-4d args: 0x%016lx 0x%016lx 0x%016lx 0x%016lx 0x%016lx 0x%016lx\n",
                arm_nr, get_syscall_name(arm_nr), x86_nr,
                ctx.x[0], ctx.x[1], ctx.x[2], ctx.x[3], ctx.x[4], ctx.x[5]);
    }

    // Route to custom handler if needed
    switch (arm_nr) {
        case 63:  return handle_read(ctx);
        case 64:  return handle_write(ctx);
        case 222: return handle_mmap(ctx);
        case 226: return handle_mprotect(ctx);
        case 215: return handle_munmap(ctx);
        case 214: return handle_brk(ctx);
        case 216: return handle_mremap(ctx);
        case 220: return handle_clone(ctx);
        case 435: return handle_clone3(ctx);
        case 98:  return handle_futex(ctx);
        case 93:  return handle_exit(ctx);
        case 94:  return handle_exit_group(ctx);
        case 96:  return handle_set_tid_address(ctx);
        case 100: return handle_set_robust_list(ctx);
        case 178: return handle_gettid(ctx);
        case 167: return handle_prctl(ctx);
        case 139: return handle_rt_sigreturn(ctx);
        case 278: return handle_getrandom(ctx);
        case 69:  return handle_preadv(ctx);
        case 70:  return handle_pwritev(ctx);
        case 71:  return handle_sendfile(ctx);
        case 76:  return handle_splice(ctx);
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
    ctx.exit_status = static_cast<int>(ctx.x[0]);
    ctx.state_flags |= CPUContext::STATE_EXITED;
    return ctx.exit_status;
}

int64_t SyscallHandler::handle_exit_group(CPUContext& ctx) {
    ctx.exit_status = static_cast<int>(ctx.x[0]);
    ctx.state_flags |= CPUContext::STATE_EXITED;
    return ctx.exit_status;
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

int64_t SyscallHandler::handle_mremap(CPUContext& ctx) {
    uint64_t old_addr = ctx.x[0];
    size_t old_len = ctx.x[1];
    size_t new_len = ctx.x[2];
    int flags = static_cast<int>(ctx.x[3]);
    uint64_t new_addr = ctx.x[4];
    
    // Invalidate any translated pages in the old region
    mem_.invalidate_translated_pages();
    
    return static_cast<int64_t>(
        syscall(SYS_mremap, old_addr, old_len, new_len, flags, new_addr)
    );
}

int64_t SyscallHandler::handle_clone3(CPUContext& ctx) {
    void* cl_args = reinterpret_cast<void*>(ctx.x[0]);
    size_t size = ctx.x[1];
    // clone3 is ABI compatible between ARM64 and x86_64
    return static_cast<int64_t>(
        syscall(SYS_clone3, cl_args, size)
    );
}

int64_t SyscallHandler::handle_preadv(CPUContext& ctx) {
    int fd = static_cast<int>(ctx.x[0]);
    const struct iovec* iov = reinterpret_cast<const struct iovec*>(ctx.x[1]);
    int iovcnt = static_cast<int>(ctx.x[2]);
    off_t offset = static_cast<off_t>(ctx.x[3]);
    return static_cast<int64_t>(
        syscall(SYS_preadv, fd, iov, iovcnt, offset)
    );
}

int64_t SyscallHandler::handle_pwritev(CPUContext& ctx) {
    int fd = static_cast<int>(ctx.x[0]);
    const struct iovec* iov = reinterpret_cast<const struct iovec*>(ctx.x[1]);
    int iovcnt = static_cast<int>(ctx.x[2]);
    off_t offset = static_cast<off_t>(ctx.x[3]);
    return static_cast<int64_t>(
        syscall(SYS_pwritev, fd, iov, iovcnt, offset)
    );
}

int64_t SyscallHandler::handle_sendfile(CPUContext& ctx) {
    int out_fd = static_cast<int>(ctx.x[0]);
    int in_fd = static_cast<int>(ctx.x[1]);
    off_t* offset = reinterpret_cast<off_t*>(ctx.x[2]);
    size_t count = ctx.x[3];
    return static_cast<int64_t>(
        syscall(SYS_sendfile, out_fd, in_fd, offset, count)
    );
}

int64_t SyscallHandler::handle_splice(CPUContext& ctx) {
    int fd_in = static_cast<int>(ctx.x[0]);
    off_t* off_in = reinterpret_cast<off_t*>(ctx.x[1]);
    int fd_out = static_cast<int>(ctx.x[2]);
    off_t* off_out = reinterpret_cast<off_t*>(ctx.x[3]);
    size_t len = ctx.x[4];
    unsigned int flags = static_cast<unsigned int>(ctx.x[5]);
    return static_cast<int64_t>(
        syscall(SYS_splice, fd_in, off_in, fd_out, off_out, len, flags)
    );
}

bool SyscallHandler::copy_guest_to_host(uint64_t guest_addr, void* host_buf, size_t len) {
    return mem_.read_bytes(guest_addr, host_buf, len);
}

bool SyscallHandler::copy_host_to_guest(uint64_t guest_addr, const void* host_buf, size_t len) {
    return mem_.write_bytes(guest_addr, host_buf, len);
}

int64_t SyscallHandler::handle_write(CPUContext& ctx) {
    int fd = static_cast<int>(ctx.x[0]);
    uint64_t guest_buf = ctx.x[1];
    size_t count = ctx.x[2];
    
    // Allocate temporary buffer on stack for small writes
    uint8_t stack_buf[4096];
    uint8_t* buf = stack_buf;
    bool allocated = false;
    
    if (count > sizeof(stack_buf)) {
        buf = static_cast<uint8_t*>(malloc(count));
        allocated = true;
        if (!buf) return -ENOMEM;
    }
    
    if (!mem_.read_bytes(guest_buf, buf, count)) {
        if (allocated) free(buf);
        return -EFAULT;
    }
    
    ssize_t result = write(fd, buf, count);
    
    if (allocated) free(buf);
    return static_cast<int64_t>(result);
}

int64_t SyscallHandler::handle_read(CPUContext& ctx) {
    int fd = static_cast<int>(ctx.x[0]);
    uint64_t guest_buf = ctx.x[1];
    size_t count = ctx.x[2];
    
    uint8_t stack_buf[4096];
    uint8_t* buf = stack_buf;
    bool allocated = false;
    
    if (count > sizeof(stack_buf)) {
        buf = static_cast<uint8_t*>(malloc(count));
        allocated = true;
        if (!buf) return -ENOMEM;
    }
    
    ssize_t result = read(fd, buf, count);
    
    if (result > 0) {
        if (!mem_.write_bytes(guest_buf, buf, result)) {
            if (allocated) free(buf);
            return -EFAULT;
        }
    }
    
    if (allocated) free(buf);
    return static_cast<int64_t>(result);
}

int64_t SyscallHandler::passthrough_syscall(int x86_nr, uint64_t arg0, uint64_t arg1,
                                             uint64_t arg2, uint64_t arg3,
                                             uint64_t arg4, uint64_t arg5) {
    return static_cast<int64_t>(
        syscall(x86_nr, arg0, arg1, arg2, arg3, arg4, arg5)
    );
}

} // namespace bbarm64
