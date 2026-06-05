#pragma once
#include <cstdio>
#include <utility>

namespace prism {

enum class LogLevel {
    Debug,
    Info,
    Warn,
    Error,
};

inline const char* logLevelName(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
    }
    return "UNKNOWN";
}

inline void log(LogLevel level, const char* mod, const char* fmt, auto... args) {
    std::fprintf(stderr, "[Prism][%s][%s] ", logLevelName(level), mod);
    std::fprintf(stderr, fmt, args...);
    std::fputc('\n', stderr);
}

} // namespace prism

#define PRISM_LOG(level, fmt, ...) \
    prism::log(prism::LogLevel::level, "unknown", fmt, ##__VA_ARGS__)

#define PRISM_MOD_LOG(level, fmt, ...) \
    prism::log(prism::LogLevel::level, getInfo().id.data(), fmt, ##__VA_ARGS__)
