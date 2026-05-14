#include "util/logger.h"

#include <chrono>
#include <iomanip>

namespace macro::log {

Logger& Logger::instance() {
    static Logger l;
    return l;
}

static const char* tag(Level lvl) {
    switch (lvl) {
        case Level::Debug: return "DEBUG";
        case Level::Info:  return "INFO ";
        case Level::Warn:  return "WARN ";
        case Level::Error: return "ERROR";
    }
    return "?    ";
}

void Logger::log(Level lvl, const std::string& msg) {
    if (lvl < min_level_) return;
    std::lock_guard<std::mutex> lk(mu_);
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    auto& out = (lvl >= Level::Warn) ? std::cerr : std::clog;
    out << std::put_time(&tm, "%FT%TZ") << " [" << tag(lvl) << "] " << msg << "\n";
}

} // namespace macro::log
