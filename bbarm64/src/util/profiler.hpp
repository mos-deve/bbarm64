#pragma once
#include <cstdint>
#include <unordered_map>
#include <chrono>

namespace bbarm64 {

// Performance profiler
class Profiler {
public:
    static Profiler& instance() {
        static Profiler inst;
        return inst;
    }

    Profiler();

    // Record a block execution
    void record_block(uint64_t arm_pc, uint32_t instr_count);

    // Record a syscall
    void record_syscall(int nr);

    // Print stats
    void print_stats() const;

    // Reset
    void reset();

private:
    struct BlockStats {
        uint64_t arm_pc;
        uint32_t hit_count;
        uint32_t instr_count;
    };

    std::unordered_map<uint64_t, BlockStats> block_stats_;
    std::unordered_map<int, uint64_t> syscall_counts_;
    uint64_t total_blocks_;
    uint64_t total_syscalls_;
    std::chrono::steady_clock::time_point start_time_;
};

} // namespace bbarm64
