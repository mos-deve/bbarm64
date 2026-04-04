#include "thread_manager.hpp"
#include "../core/exec_engine.hpp"
#include "../syscall/syscall_handlers.hpp"
#include <sys/syscall.h>
#include <unistd.h>
#include <cstdio>

namespace bbarm64 {

ThreadManager::ThreadManager(ExecEngine& engine, SyscallHandler& syscall_handler)
    : engine_(engine), syscall_handler_(syscall_handler), next_tid_(1) {}

int ThreadManager::create_thread(uint64_t flags, void* stack, int* ptid,
                                   void* tls, int* ctid, const CPUContext& parent_ctx) {
    std::lock_guard<std::mutex> lock(mutex_);

    ThreadInfo info;
    info.tid = next_tid_++;
    info.ctx = parent_ctx;
    info.ctx.sp = reinterpret_cast<uint64_t>(stack);
    info.ctx.tpidr_el0 = reinterpret_cast<uint64_t>(tls);
    info.ctx.tid = info.tid;
    info.active = true;

    // Set up child return value (0 for child)
    info.ctx.x[0] = 0;

    info.thread = std::thread(thread_entry, this, &threads_.emplace_back(std::move(info)));

    if (ptid) *ptid = static_cast<int>(info.tid);

    return static_cast<int>(info.tid);
}

void ThreadManager::exit_thread(int status) {
    (void)status;
    // Thread will exit naturally when its function returns
}

uint64_t ThreadManager::current_tid() const {
    return 0;  // Stub - would use thread-local storage
}

CPUContext* ThreadManager::get_context(uint64_t tid) {
    for (auto& t : threads_) {
        if (t.tid == tid && t.active) return &t.ctx;
    }
    return nullptr;
}

void ThreadManager::set_tls(uint64_t tpidr) {
    // Set FS base for current thread (x86_64 TLS)
    // This maps ARM64 TPIDR_EL0 to x86_64 FS segment
    (void)tpidr;
}

uint64_t ThreadManager::get_tls() const {
    return 0;  // Stub
}

void ThreadManager::join_all() {
    for (auto& t : threads_) {
        if (t.thread.joinable()) {
            t.thread.join();
        }
    }
}

void ThreadManager::thread_entry(ThreadManager* mgr, ThreadInfo* info) {
    // Set TLS for this thread
    mgr->set_tls(info->ctx.tpidr_el0);

    // Start executing from the thread's PC
    mgr->engine_.run(info->ctx.pc);

    info->active = false;
}

} // namespace bbarm64
