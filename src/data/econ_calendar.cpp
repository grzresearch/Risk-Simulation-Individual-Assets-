#include "data/econ_calendar.h"

#include "util/logger.h"

#include <algorithm>

namespace macro::data {

std::vector<MacroEvent>
EconCalendar::upcoming(const std::vector<EventKind>& kinds,
                       const std::string& from_iso,
                       const std::string& to_iso) {
    std::vector<MacroEvent> out;
    for (auto k : kinds) {
        auto map = fred_mapping(k);
        std::vector<MacroEvent> evs;
        if (fred_.has_key()) {
            evs = fred_.fetch_release_calendar(map.release_id, k, from_iso, to_iso);
        }
        if (evs.empty()) {
            log::info(std::string("Calendar fallback for ") + to_string(k));
            evs = fallback(k, from_iso, to_iso);
        }
        out.insert(out.end(), evs.begin(), evs.end());
    }
    std::sort(out.begin(), out.end(),
              [](const MacroEvent& a, const MacroEvent& b) { return a.date < b.date; });
    return out;
}

// Rule-of-thumb release dates when FRED is unavailable. These are nominal
// approximations: real dates shift by 1-2 business days but the analysis only
// needs the month/week alignment.
std::vector<MacroEvent>
EconCalendar::fallback(EventKind k,
                       const std::string& from,
                       const std::string& to) {
    std::vector<MacroEvent> out;
    auto start = time::parse_date(from);
    auto end   = time::parse_date(to);

    auto add = [&](time::Date d) {
        if (d < start || d > end) return;
        MacroEvent ev;
        ev.kind   = k;
        ev.label  = std::string(to_string(k)) + " (estimated)";
        ev.date   = d;
        ev.source = "heuristic";
        out.push_back(ev);
    };

    // Iterate month by month
    auto cur = start;
    while (cur <= end) {
        auto t = std::chrono::system_clock::to_time_t(cur);
        std::tm tm{};
#ifdef _WIN32
        gmtime_s(&tm, &t);
#else
        gmtime_r(&t, &tm);
#endif
        int y = tm.tm_year + 1900;
        int m = tm.tm_mon + 1;

        auto mk = [&](int day) {
            std::tm out_tm{};
            out_tm.tm_year = y - 1900;
            out_tm.tm_mon  = m - 1;
            out_tm.tm_mday = day;
#ifdef _WIN32
            return std::chrono::system_clock::from_time_t(_mkgmtime(&out_tm));
#else
            return std::chrono::system_clock::from_time_t(timegm(&out_tm));
#endif
        };

        switch (k) {
            case EventKind::CPI:               add(mk(12)); break; // ~mid-month
            case EventKind::CorePCE:           add(mk(28)); break;
            case EventKind::PPI:               add(mk(13)); break;
            case EventKind::NonFarmPayrolls:   add(mk(5));  break; // 1st Friday-ish
            case EventKind::UnemploymentRate:  add(mk(5));  break;
            case EventKind::FOMC:
                // FOMC: 8 meetings/year, roughly every 6 weeks. Hit Jan/Mar/May/Jun/Jul/Sep/Nov/Dec
                if (m == 1 || m == 3 || m == 5 || m == 6 || m == 7 || m == 9 || m == 11 || m == 12)
                    add(mk(18));
                break;
            case EventKind::GDP:               if (m % 3 == 1) add(mk(27)); break;
            case EventKind::RetailSales:       add(mk(15)); break;
            case EventKind::ISM_Manufacturing: add(mk(1));  break;
            case EventKind::ConsumerSentiment: add(mk(14)); break;
            case EventKind::Unknown: break;
        }
        // advance ~30 days
        cur = time::add_days(cur, 30);
    }
    return out;
}

} // namespace macro::data
