#pragma once
#include <cstdint>

namespace bbarm64 {

// ARM64 syscall number -> x86_64 syscall number mapping
struct SyscallEntry {
    int arm_nr;
    int x86_nr;
    const char* name;
};

// Core syscall table (~40 most common syscalls)
static const SyscallEntry syscall_table[] = {
    // I/O
    {63, 0, "read"},
    {64, 1, "write"},
    {65, 19, "readv"},
    {66, 20, "writev"},
    {67, 17, "pread64"},
    {68, 18, "pwrite64"},
    // File
    {56, 2, "close"},
    {57, 0, "open"},  // mapped to openat
    {257, 257, "openat"},
    {0, 0, "readlinkat"},
    {21, 21, "access"},
    {48, 72, "fcntl"},
    {29, 16, "ioctl"},
    {5, 5, "fstat"},
    {79, 262, "fstatat"},
    {262, 258, "faccessat"},
    {61, 78, "getdents64"},
    // Memory
    {222, 9, "mmap"},
    {226, 10, "mprotect"},
    {215, 11, "munmap"},
    {214, 12, "brk"},
    {216, 25, "mremap"},
    {218, 28, "madvise"},
    // Thread
    {220, 56, "clone"},
    {435, 435, "clone3"},
    {93, 60, "exit"},
    {94, 231, "exit_group"},
    {96, 218, "set_tid_address"},
    {98, 202, "futex"},
    {100, 300, "set_robust_list"},
    {99, 273, "get_robust_list"},
    {178, 186, "gettid"},
    {127, 62, "kill"},
    {129, 200, "tgkill"},
    {128, 200, "tkill"},
    // Signal
    {134, 13, "rt_sigaction"},
    {135, 14, "rt_sigprocmask"},
    {139, 15, "rt_sigreturn"},
    {132, 131, "sigaltstack"},
    // Time
    {113, 228, "clock_gettime"},
    {114, 229, "clock_getres"},
    {101, 35, "nanosleep"},
    // Polling
    {20, 21, "epoll_create1"},
    {21, 233, "epoll_ctl"},
    {22, 22, "epoll_pwait"},
    {72, 23, "ppoll"},
    // Process
    {172, 39, "getpid"},
    {173, 110, "getppid"},
    {174, 102, "getuid"},
    {176, 104, "geteuid"},
    {177, 107, "getegid"},
    {167, 157, "prctl"},
    // Network
    {198, 41, "socket"},
    {200, 49, "bind"},
    {201, 50, "listen"},
    {202, 43, "accept"},
    {203, 42, "connect"},
    {206, 44, "sendto"},
    {207, 45, "recvfrom"},
    {209, 54, "setsockopt"},
    {210, 55, "getsockopt"},
    // Misc
    {160, 158, "uname"},
    {278, 318, "getrandom"},
    {283, 324, "membarrier"},
};

// Translate ARM64 syscall number to x86_64
int translate_syscall_nr(int arm_nr);

// Get syscall name
const char* get_syscall_name(int arm_nr);

} // namespace bbarm64
