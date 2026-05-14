#pragma once

#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

namespace macro::log {

enum class Level { Debug, Info, Warn, Error };

class Logger {
public:
    static Logger& instance();
    void set_level(Level lvl) { min_level_ = lvl; }
    void log(Level lvl, const std::string& msg);

private:
    Logger() = default;
    Level min_level_ = Level::Info;
    std::mutex mu_;
};

inline void debug(const std::string& m) { Logger::instance().log(Level::Debug, m); }
inline void info (const std::string& m) { Logger::instance().log(Level::Info,  m); }
inline void warn (const std::string& m) { Logger::instance().log(Level::Warn,  m); }
inline void error(const std::string& m) { Logger::instance().log(Level::Error, m); }

} // namespace macro::log
