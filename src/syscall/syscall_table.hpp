#pragma once
#include <cstdint>

namespace bbarm64 {

// ARM64 syscall number -> x86_64 syscall number mapping
struct SyscallEntry {
    int arm_nr;
    int x86_nr;
    const char* name;
};

// Core syscall table (~120 most common syscalls)
static const SyscallEntry syscall_table[] = {
    // Core I/O syscalls
    {63, 0, "read"},
    {64, 1, "write"},
    {65, 19, "readv"},
    {66, 20, "writev"},
    {67, 17, "pread64"},
    {68, 18, "pwrite64"},
    {69, 295, "preadv"},
    {70, 296, "pwritev"},
    {62, 8, "lseek"},
    {71, 40, "sendfile"},
    {75, 278, "vmsplice"},
    {76, 275, "splice"},
    {77, 276, "tee"},
    
    // File operations
    {56, 3, "close"},
    {57, 3, "close"},
    {257, 257, "openat"},
    {23, 32, "dup"},
    {24, 292, "dup3"},
    {59, 293, "pipe2"},
    {48, 72, "fcntl"},
    {29, 16, "ioctl"},
    {32, 73, "flock"},
    {5, 5, "fstat"},
    {79, 262, "newfstatat"},
    {46, 77, "ftruncate"},
    {47, 285, "fallocate"},
    {61, 78, "getdents64"},
    {81, 162, "sync"},
    {82, 74, "fsync"},
    {83, 75, "fdatasync"},
    {84, 277, "sync_file_range"},
    {88, 280, "utimensat"},
    
    // Directory operations
    {33, 259, "mknodat"},
    {34, 258, "mkdirat"},
    {35, 263, "unlinkat"},
    {36, 266, "symlinkat"},
    {37, 265, "linkat"},
    {38, 264, "renameat"},
    {266, 267, "renameat2"},
    {78, 267, "readlinkat"},
    {49, 80, "chdir"},
    {50, 81, "fchdir"},
    
    // Memory management
    {222, 9, "mmap"},
    {226, 10, "mprotect"},
    {215, 11, "munmap"},
    {214, 12, "brk"},
    {216, 25, "mremap"},
    {218, 28, "madvise"},
    {219, 26, "msync"},
    {221, 27, "mlock"},
    
    // Thread/Process management
    {220, 56, "clone"},
    {435, 435, "clone3"},
    {93, 60, "exit"},
    {94, 231, "exit_group"},
    {95, 247, "waitid"},
    {96, 218, "set_tid_address"},
    {97, 272, "unshare"},
    {98, 202, "futex"},
    {99, 273, "get_robust_list"},
    {100, 300, "set_robust_list"},
    {178, 186, "gettid"},
    {127, 62, "kill"},
    {128, 200, "tkill"},
    {129, 200, "tgkill"},
    {166, 156, "arch_prctl"},
    
    // Signal handling
    {132, 131, "sigaltstack"},
    {134, 13, "rt_sigaction"},
    {135, 14, "rt_sigprocmask"},
    {130, 128, "rt_sigtimedwait"},
    {131, 129, "rt_sigqueueinfo"},
    {133, 130, "rt_sigsuspend"},
    {138, 132, "rt_tgsigqueueinfo"},
    {139, 15, "rt_sigreturn"},
    
    // Time and timers
    {101, 35, "nanosleep"},
    {113, 228, "clock_gettime"},
    {114, 229, "clock_getres"},
    {115, 230, "clock_settime"},
    {116, 232, "clock_nanosleep"},
    {85, 283, "timerfd_create"},
    {86, 286, "timerfd_settime"},
    {87, 287, "timerfd_gettime"},
    
    // Polling and event handling
    {20, 294, "epoll_create1"},
    {21, 233, "epoll_ctl"},
    {22, 22, "epoll_pwait"},
    {72, 270, "pselect6"},
    {73, 23, "ppoll"},
    {26, 294, "inotify_init1"},
    {27, 254, "inotify_add_watch"},
    {28, 255, "inotify_rm_watch"},
    {19, 290, "eventfd2"},
    {74, 289, "signalfd4"},
    
    // Process identity
    {172, 39, "getpid"},
    {173, 110, "getppid"},
    {174, 102, "getuid"},
    {175, 105, "getgid"},
    {176, 104, "geteuid"},
    {177, 107, "getegid"},
    {169, 115, "getgroups"},
    {170, 116, "setgroups"},
    
    // Network syscalls
    {198, 41, "socket"},
    {200, 49, "bind"},
    {201, 50, "listen"},
    {202, 43, "accept"},
    {203, 42, "connect"},
    {192, 46, "sendmsg"},
    {191, 47, "recvmsg"},
    {206, 44, "sendto"},
    {207, 45, "recvfrom"},
    {209, 54, "setsockopt"},
    {210, 55, "getsockopt"},
    {208, 51, "shutdown"},
    
    // Kernel key management
    {217, 248, "add_key"},
    {221, 27, "mlock"},
    {223, 29, "munlock"},
    {224, 30, "mlockall"},
    {225, 31, "munlockall"},
    {227, 32, "mincore"},
    
    // Miscellaneous
    {160, 158, "uname"},
    {167, 157, "prctl"},
    {17, 79, "getcwd"},
    {278, 318, "getrandom"},
    {283, 324, "membarrier"},
    {280, 321, "bpf"},
    {92, 135, "personality"},
    {90, 125, "capget"},
    {91, 126, "capset"},
};

// Translate ARM64 syscall number to x86_64
int translate_syscall_nr(int arm_nr);

// Get syscall name
const char* get_syscall_name(int arm_nr);

} // namespace bbarm64
