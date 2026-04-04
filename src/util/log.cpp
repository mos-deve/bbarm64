#include "log.hpp"

namespace bbarm64 {

void Logger::log(LogLevel level, const char* fmt, ...) {
    if (level > level_) return;

    const char* prefix = "";
    switch (level) {
        case LogLevel::ERROR_: prefix = "[ERROR]"; break;
        case LogLevel::WARN:   prefix = "[WARN] "; break;
        case LogLevel::INFO:   prefix = "[INFO] "; break;
        case LogLevel::DEBUG:  prefix = "[DEBUG]"; break;
        case LogLevel::TRACE:  prefix = "[TRACE]"; break;
    }

    fprintf(stderr, "%s ", prefix);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

} // namespace bbarm64
