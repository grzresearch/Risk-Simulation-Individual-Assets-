#include "util/json_logger.h"

#include "util/version.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace macro::log {

JsonLogger& JsonLogger::instance() { static JsonLogger l; return l; }

void JsonLogger::set_sink(std::ostream* s) { sink_ = s; }

namespace {
std::string iso_now() {
    auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    std::ostringstream o;
    o << std::put_time(&tm, "%FT%TZ");
    return o.str();
}

std::string escape(const std::string& s) {
    std::string out; out.reserve(s.size() + 4);
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8]; std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else out += c;
        }
    }
    return out;
}

void emit_value(std::ostringstream& o, const FieldValue& v) {
    if (auto ps = std::get_if<std::string>(&v))     { o << '"' << escape(*ps) << '"'; }
    else if (auto pd = std::get_if<double>(&v))     { o << *pd; }
    else if (auto pi = std::get_if<long long>(&v))  { o << *pi; }
    else if (auto pb = std::get_if<bool>(&v))       { o << (*pb ? "true" : "false"); }
}
} // namespace

void JsonLogger::log(const std::string& level,
                     const std::string& event,
                     const std::map<std::string, FieldValue>& fields) {
    if (!enabled_) return;
    std::ostringstream o;
    o << "{\"ts\":\"" << iso_now() << "\""
      << ",\"level\":\"" << level << "\""
      << ",\"event\":\"" << escape(event) << "\""
      << ",\"version\":\"" << version() << "\""
      << ",\"git\":\"" << git_sha() << "\"";
    for (const auto& [k, v] : fields) {
        o << ",\"" << escape(k) << "\":";
        emit_value(o, v);
    }
    o << "}\n";
    std::lock_guard<std::mutex> lk(mu_);
    auto& out = sink_ ? *sink_ : std::clog;
    out << o.str();
    out.flush();
}

} // namespace macro::log
