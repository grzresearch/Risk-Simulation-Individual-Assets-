#pragma once

#include <map>
#include <mutex>
#include <ostream>
#include <string>
#include <variant>

namespace macro::log {

using FieldValue = std::variant<std::string, double, long long, bool>;

// Structured single-line JSON logger. Fields are flushed atomically to the
// configured sink (default: std::clog). Each entry is one JSON object.
class JsonLogger {
public:
    static JsonLogger& instance();
    void set_sink(std::ostream* s);
    void enable(bool on) { enabled_ = on; }

    void log(const std::string& level,
             const std::string& event,
             const std::map<std::string, FieldValue>& fields = {});

private:
    JsonLogger() = default;
    bool enabled_ = false;
    std::ostream* sink_ = nullptr;
    std::mutex mu_;
};

inline void event(const std::string& level,
                  const std::string& name,
                  const std::map<std::string, FieldValue>& f = {}) {
    JsonLogger::instance().log(level, name, f);
}

} // namespace macro::log
