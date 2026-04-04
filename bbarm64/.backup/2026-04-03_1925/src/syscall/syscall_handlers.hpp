#pragma once
#include <cstdint>
#include "../core/cpu_context.hpp"

namespace bbarm64 {

class MemoryManager;

// Syscall handler - executes translated syscalls on the host
class SyscallHandler {
public:
    SyscallHandler(MemoryManager& mem);

    // Execute a syscall with ARM64 arguments, return result
    int64_t execute(CPUContext& ctx);

private:
    MemoryManager& mem_;

    // Custom handlers for syscalls that need special treatment
    int64_t handle_mmap(CPUContext& ctx);
    int64_t handle_mprotect(CPUContext& ctx);
    int64_t handle_munmap(CPUContext& ctx);
    int64_t handle_brk(CPUContext& ctx);
    int64_t handle_clone(CPUContext& ctx);
    int64_t handle_futex(CPUContext& ctx);
    int64_t handle_exit(CPUContext& ctx);
    int64_t handle_exit_group(CPUContext& ctx);
    int64_t handle_set_tid_address(CPUContext& ctx);
    int64_t handle_set_robust_list(CPUContext& ctx);
    int64_t handle_gettid(CPUContext& ctx);
    int64_t handle_prctl(CPUContext& ctx);
    int64_t handle_arch_prctl(CPUContext& ctx);
    int64_t handle_rt_sigreturn(CPUContext& ctx);
    int64_t handle_getrandom(CPUContext& ctx);
    int64_t handle_mmap2(CPUContext& ctx);

    // Generic pass-through syscall
    int64_t passthrough_syscall(int x86_nr, uint64_t arg0, uint64_t arg1,
                                 uint64_t arg2, uint64_t arg3, uint64_t arg4,
                                 uint64_t arg5);
};

} // namespace bbarm64
