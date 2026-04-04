#pragma once
#include <cstdint>
#include <thread>
#include <mutex>
#include <vector>
#include "../core/cpu_context.hpp"

namespace bbarm64 {

class ExecEngine;
class SyscallHandler;

// Thread manager - 1:1 mapping of ARM64 threads to host pthreads
class ThreadManager {
public:
    ThreadManager(ExecEngine& engine, SyscallHandler& syscall_handler);

    // Create a new thread (from clone syscall)
    int create_thread(uint64_t flags, void* stack, int* ptid, void* tls, int* ctid,
                      const CPUContext& parent_ctx);

    // Exit current thread
    void exit_thread(int status);

    // Get current thread ID
    uint64_t current_tid() const;

    // Get thread context by TID
    CPUContext* get_context(uint64_t tid);

    // Set TLS for current thread
    void set_tls(uint64_t tpidr);

    // Get TLS for current thread
    uint64_t get_tls() const;

    // Wait for all threads to finish
    void join_all();

private:
    struct ThreadInfo {
        uint64_t tid;
        std::thread thread;
        CPUContext ctx;
        bool active;
    };

    ExecEngine& engine_;
    SyscallHandler& syscall_handler_;
    std::vector<ThreadInfo> threads_;
    mutable std::mutex mutex_;
    uint64_t next_tid_;

    static void thread_entry(ThreadManager* mgr, ThreadInfo* info);
};

} // namespace bbarm64
