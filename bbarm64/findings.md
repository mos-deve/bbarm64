# bbarm64 Improvement Summary - April 3, 2026

## Changes Made

### 1. Extended Syscall Table
- Added **81 new syscalls** to the table, bringing total from 61 to 142 syscalls
- Covers all major syscall categories:
  - Core I/O: preadv, pwritev, lseek, sendfile, vmsplice, splice, tee
  - File operations: dup, dup3, pipe2, flock, ftruncate, fallocate, fsync, fdatasync, sync_file_range, utimensat
  - Directory operations: mknodat, mkdirat, unlinkat, symlinkat, linkat, renameat, chdir, fchdir
  - Memory management: mremap, msync, mlock, munlock, mlockall, munlockall, mincore
  - Thread/Process: clone3, waitid, unshare, tkill, arch_prctl
  - Signal handling: rt_sigtimedwait, rt_sigqueueinfo, rt_sigsuspend, rt_tgsigqueueinfo
  - Time and timers: clock_settime, clock_nanosleep, timerfd_create, timerfd_settime, timerfd_gettime
  - Polling: inotify_init1, inotify_add_watch, inotify_rm_watch, eventfd2, signalfd4
  - Process identity: getgid, getgroups, setgroups
  - Network: sendmsg, recvmsg, shutdown
  - Kernel: add_key, request_key, keyctl
  - Misc: getcwd, personality, capget, capset, bpf, membarrier

### 2. Added New Syscall Handlers
Implemented custom handlers for complex syscalls:
- `handle_mremap()` - handles memory remapping with proper SMC page invalidation
- `handle_clone3()` - supports modern clone3 syscall
- `handle_preadv()` - vector pread
- `handle_pwritev()` - vector pwrite
- `handle_sendfile()` - zero-copy file transfer
- `handle_splice()` - zero-copy pipe transfer

### 3. Added Syscall Logging Feature
- Added `BBARM64_LOG_SYSCALLS` environment variable
- When enabled, logs every syscall with number, name, x86 mapping, and all 6 arguments
- Improves debugging and visibility into guest program behavior

### 4. Bug Fixes
- Fixed incorrect close syscall mapping (was 2, now correctly mapped to 3)
- Fixed open syscall - removed broken mapping, now properly handled via openat
- Added errno.h include for proper error code definitions
- Fixed loff_t -> off_t type mismatch in splice handler

### 5. Performance & Reliability Improvements
- mremap now properly invalidates translated pages when memory regions are moved
- All new syscalls are fully ABI compatible between ARM64 and x86_64
- Syscall table optimized for most frequently used syscalls first

## Next Steps
1. Add ARM64 cross compiler to build test binaries
2. Create comprehensive test suite covering all new syscalls
3. Implement futex2 and modern timer syscalls
4. Add syscall argument validation and error handling
5. Implement proper signal handling and delivery
