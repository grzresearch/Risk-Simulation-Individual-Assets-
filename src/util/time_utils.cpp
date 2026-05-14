#include "util/time_utils.h"

#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace macro::time {

Date today_utc() {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    tm.tm_hour = 0; tm.tm_min = 0; tm.tm_sec = 0;
#ifdef _WIN32
    auto tt = _mkgmtime(&tm);
#else
    auto tt = timegm(&tm);
#endif
    return std::chrono::system_clock::from_time_t(tt);
}

Date parse_date(const std::string& s) {
    std::tm tm{};
    std::istringstream iss(s);
    iss >> std::get_time(&tm, "%Y-%m-%d");
    if (iss.fail()) {
        throw std::runtime_error("invalid date: " + s);
    }
#ifdef _WIN32
    auto tt = _mkgmtime(&tm);
#else
    auto tt = timegm(&tm);
#endif
    return std::chrono::system_clock::from_time_t(tt);
}

std::string to_iso(Date d) {
    auto t = std::chrono::system_clock::to_time_t(d);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d");
    return oss.str();
}

Date add_days(Date d, int days) {
    return d + std::chrono::hours(24 * days);
}

int diff_days(Date a, Date b) {
    using namespace std::chrono;
    return static_cast<int>(duration_cast<hours>(a - b).count() / 24);
}

bool is_weekend(Date d) {
    auto t = std::chrono::system_clock::to_time_t(d);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    return tm.tm_wday == 0 || tm.tm_wday == 6;
}

std::optional<Date> next_business_day(Date d) {
    for (int i = 1; i <= 7; ++i) {
        auto nd = add_days(d, i);
        if (!is_weekend(nd)) return nd;
    }
    return std::nullopt;
}

} // namespace macro::time
