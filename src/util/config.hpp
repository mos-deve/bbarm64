#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

namespace bbarm64 {

// Runtime configuration from environment variables
struct Config {
    bool dump_blocks = false;       // BBARM64_DUMP=1
    bool log_syscalls = false;      // BBARM64_LOG_SYSCALLS=1
    bool log_signals = false;       // BBARM64_LOG_SIGNALS=1
    bool smc_detect = true;         // BBARM64_SMC_DETECT=1 (default on)
    uint32_t max_block_size = 64;   // BBARM64_MAX_BLOCK=64
    uint32_t cache_size_mb = 64;    // BBARM64_CACHE_SIZE=64
    std::string log_file;           // BBARM64_LOG_FILE=
    std::string cache_dir;          // BBARM64_CACHE_DIR=~/.cache/bbarm64

    static Config load();
};

} // namespace bbarm64
