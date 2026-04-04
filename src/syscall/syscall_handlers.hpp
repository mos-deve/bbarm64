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
    int64_t handle_mremap(CPUContext& ctx);
    int64_t handle_clone(CPUContext& ctx);
    int64_t handle_clone3(CPUContext& ctx);
    int64_t handle_futex(CPUContext& ctx);
    int64_t handle_exit(CPUContext& ctx);
    int64_t handle_exit_group(CPUContext& ctx);
    int64_t handle_set_tid_address(CPUContext& ctx);
    int64_t handle_set_robust_list(CPUContext& ctx);
    int64_t handle_gettid(CPUContext& ctx);
    int64_t handle_prctl(CPUContext& ctx);
    int64_t handle_arch_prctl(CPUContext& ctx);
    int64_t handle_set_thread_area(CPUContext& ctx);
    int64_t handle_rt_sigreturn(CPUContext& ctx);
    int64_t handle_getrandom(CPUContext& ctx);
    int64_t handle_mmap2(CPUContext& ctx);
    int64_t handle_read(CPUContext& ctx);
    int64_t handle_write(CPUContext& ctx);
    int64_t handle_preadv(CPUContext& ctx);
    int64_t handle_pwritev(CPUContext& ctx);
    int64_t handle_sendfile(CPUContext& ctx);
    int64_t handle_splice(CPUContext& ctx);
    
    // Helper to copy guest buffer to host
    bool copy_guest_to_host(uint64_t guest_addr, void* host_buf, std::size_t len);
    bool copy_host_to_guest(uint64_t guest_addr, const void* host_buf, std::size_t len);

    // Generic pass-through syscall
    int64_t passthrough_syscall(int x86_nr, uint64_t arg0, uint64_t arg1,
                                 uint64_t arg2, uint64_t arg3, uint64_t arg4,
                                 uint64_t arg5);
};

} // namespace bbarm64
