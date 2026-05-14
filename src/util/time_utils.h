#pragma once

#include <chrono>
#include <ctime>
#include <optional>
#include <string>

namespace macro::time {

using Date = std::chrono::system_clock::time_point;

Date today_utc();
Date parse_date(const std::string& iso_yyyy_mm_dd);
std::string to_iso(Date d);

Date add_days(Date d, int days);
int  diff_days(Date a, Date b); // a - b in days
bool is_weekend(Date d);

std::optional<Date> next_business_day(Date d);

} // namespace macro::time
