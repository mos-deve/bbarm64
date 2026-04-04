#pragma once
#include <cstdio>
#include <cstdarg>

namespace bbarm64 {

enum class LogLevel {
    ERROR_,
    WARN,
    INFO,
    DEBUG,
    TRACE,
};

class Logger {
public:
    static Logger& instance() {
        static Logger inst;
        return inst;
    }

    void set_level(LogLevel level) { level_ = level; }
    void log(LogLevel level, const char* fmt, ...);

private:
    LogLevel level_ = LogLevel::INFO;
};

#define LOG_ERROR(fmt, ...) bbarm64::Logger::instance().log(bbarm64::LogLevel::ERROR_, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) bbarm64::Logger::instance().log(bbarm64::LogLevel::WARN, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) bbarm64::Logger::instance().log(bbarm64::LogLevel::INFO, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) bbarm64::Logger::instance().log(bbarm64::LogLevel::DEBUG, fmt, ##__VA_ARGS__)
#define LOG_TRACE(fmt, ...) bbarm64::Logger::instance().log(bbarm64::LogLevel::TRACE, fmt, ##__VA_ARGS__)

} // namespace bbarm64
