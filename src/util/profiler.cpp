#include "profiler.hpp"
#include <cstdio>
#include <vector>
#include <algorithm>

namespace bbarm64 {

Profiler::Profiler() : total_blocks_(0), total_syscalls_(0) {
    start_time_ = std::chrono::steady_clock::now();
}

void Profiler::record_block(uint64_t arm_pc, uint32_t instr_count) {
    auto& stats = block_stats_[arm_pc];
    stats.arm_pc = arm_pc;
    stats.hit_count++;
    stats.instr_count = instr_count;
    total_blocks_++;
}

void Profiler::record_syscall(int nr) {
    syscall_counts_[nr]++;
    total_syscalls_++;
}

void Profiler::print_stats() const {
    auto elapsed = std::chrono::steady_clock::now() - start_time_;
    double seconds = std::chrono::duration<double>(elapsed).count();

    fprintf(stderr, "\n=== bbarm64 Profiling Stats ===\n");
    fprintf(stderr, "Total blocks executed: %lu\n", total_blocks_);
    fprintf(stderr, "Total syscalls: %lu\n", total_syscalls_);
    fprintf(stderr, "Elapsed time: %.3fs\n", seconds);
    if (seconds > 0) {
        fprintf(stderr, "Blocks/sec: %.0f\n", total_blocks_ / seconds);
    }
    fprintf(stderr, "Unique blocks: %lu\n", block_stats_.size());

    // Top 10 hot blocks
    fprintf(stderr, "\nTop 10 hot blocks:\n");
    std::vector<const BlockStats*> sorted;
    for (const auto& [pc, stats] : block_stats_) {
        sorted.push_back(&stats);
    }
    std::sort(sorted.begin(), sorted.end(), [](const BlockStats* a, const BlockStats* b) {
        return a->hit_count > b->hit_count;
    });

    int count = 0;
    for (const auto* stats : sorted) {
        if (count++ >= 10) break;
        fprintf(stderr, "  0x%08lx: %u hits, %u instrs\n",
                stats->arm_pc, stats->hit_count, stats->instr_count);
    }
}

void Profiler::reset() {
    block_stats_.clear();
    syscall_counts_.clear();
    total_blocks_ = 0;
    total_syscalls_ = 0;
    start_time_ = std::chrono::steady_clock::now();
}

} // namespace bbarm64
