#include "config.hpp"
#include <cstdlib>

namespace bbarm64 {

Config Config::load() {
    Config cfg;

    const char* dump = std::getenv("BBARM64_DUMP");
    if (dump && dump[0] == '1') cfg.dump_blocks = true;

    const char* log_sys = std::getenv("BBARM64_LOG_SYSCALLS");
    if (log_sys && log_sys[0] == '1') cfg.log_syscalls = true;

    const char* log_sig = std::getenv("BBARM64_LOG_SIGNALS");
    if (log_sig && log_sig[0] == '1') cfg.log_signals = true;

    const char* smc = std::getenv("BBARM64_SMC_DETECT");
    if (smc && smc[0] == '0') cfg.smc_detect = false;

    const char* max_block = std::getenv("BBARM64_MAX_BLOCK");
    if (max_block) cfg.max_block_size = std::strtoul(max_block, nullptr, 10);

    const char* cache_size = std::getenv("BBARM64_CACHE_SIZE");
    if (cache_size) cfg.cache_size_mb = std::strtoul(cache_size, nullptr, 10);

    const char* log_file = std::getenv("BBARM64_LOG_FILE");
    if (log_file) cfg.log_file = log_file;

    const char* cache_dir = std::getenv("BBARM64_CACHE_DIR");
    if (cache_dir) cfg.cache_dir = cache_dir;
    else {
        const char* home = std::getenv("HOME");
        if (home) cfg.cache_dir = std::string(home) + "/.cache/bbarm64";
    }

    return cfg;
}

} // namespace bbarm64
