#pragma once

#include "util/time_utils.h"

#include <set>

namespace macro::time {

// Hard-coded NYSE non-trading days for 2010-2030 plus the rule-based
// observed federal holidays. Sufficient for back-tests and forward windows
// up to ~5 years. For longer horizons swap in an exchange-specific feed.
class HolidayCalendar {
public:
    HolidayCalendar();

    bool is_holiday(Date d) const;
    bool is_trading_day(Date d) const;
    Date next_trading_day(Date d) const;
    Date prev_trading_day(Date d) const;
    Date shift_trading_days(Date d, int n) const; // signed

private:
    std::set<std::string> closed_; // ISO yyyy-mm-dd
};

} // namespace macro::time
