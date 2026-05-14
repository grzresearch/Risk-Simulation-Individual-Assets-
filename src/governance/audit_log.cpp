#include "governance/audit_log.h"

#include "util/logger.h"
#include "util/version.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>

namespace macro::governance {

namespace {
std::mutex& mu() { static std::mutex m; return m; }

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
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8]; std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else out += c;
        }
    }
    return out;
}
} // namespace

AuditLog::AuditLog(std::string path) : path_(std::move(path)) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(fs::path(path_).parent_path(), ec);
}

void AuditLog::record(const std::string& actor,
                      const std::string& action,
                      const std::map<std::string, std::string>& fields) {
    std::ostringstream o;
    o << "{\"ts\":\"" << iso_now() << "\""
      << ",\"actor\":\"" << escape(actor) << "\""
      << ",\"action\":\"" << escape(action) << "\""
      << ",\"version\":\"" << version() << "\""
      << ",\"git\":\"" << git_sha() << "\"";
    for (const auto& [k, v] : fields) {
        o << ",\"" << escape(k) << "\":\"" << escape(v) << "\"";
    }
    o << "}\n";

    std::lock_guard<std::mutex> lk(mu());
    std::ofstream f(path_, std::ios::app);
    if (!f) {
        log::warn("audit: failed to open " + path_);
        return;
    }
    f << o.str();
}

} // namespace macro::governance
