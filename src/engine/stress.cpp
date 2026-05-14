#include "engine/stress.h"

#include <algorithm>
#include <cmath>

namespace macro::engine {

std::vector<StressScenario> default_scenarios() {
    return {
        {"GFC peak panic",     "Lehman / TARP shock",                      time::parse_date("2008-09-01"), time::parse_date("2008-12-31")},
        {"Euro crisis 2011",   "US debt-ceiling + Eurozone contagion",     time::parse_date("2011-07-15"), time::parse_date("2011-10-15")},
        {"Taper tantrum 2013", "Bernanke taper signal",                    time::parse_date("2013-05-15"), time::parse_date("2013-09-15")},
        {"China devaluation",  "PBoC CNY devaluation + equity flash",      time::parse_date("2015-08-01"), time::parse_date("2015-09-30")},
        {"Volmageddon 2018",   "XIV implosion + risk-parity unwind",       time::parse_date("2018-02-01"), time::parse_date("2018-02-28")},
        {"Q4 2018 selloff",    "Powell pivot pressure",                    time::parse_date("2018-10-01"), time::parse_date("2018-12-31")},
        {"Covid crash",        "March 2020 liquidity cascade",             time::parse_date("2020-02-19"), time::parse_date("2020-03-31")},
        {"2022 CPI shock",     "Hot June CPI -> 75bp + bond/equity dump",  time::parse_date("2022-06-01"), time::parse_date("2022-09-30")},
        {"SVB / regional bk",  "Bank-run + AT1 wipeout",                   time::parse_date("2023-03-08"), time::parse_date("2023-03-31")},
    };
}

StressResult replay(const StressScenario& s,
                    const std::string& asset_id,
                    const PriceSeries& prices) {
    StressResult r;
    r.scenario  = s;
    r.asset_id  = asset_id;
    if (prices.empty()) return r;

    auto begin = std::lower_bound(prices.begin(), prices.end(), s.start,
        [](const PricePoint& p, time::Date d) { return p.date < d; });
    auto end   = std::upper_bound(prices.begin(), prices.end(), s.end,
        [](time::Date d, const PricePoint& p) { return d < p.date; });
    if (begin == prices.end() || begin == end) return r;

    double p0 = begin->close;
    double peak = p0;
    double mdd = 0.0;
    auto last = begin;
    for (auto it = begin; it != end; ++it) {
        peak = std::max(peak, it->close);
        if (peak > 0) {
            double dd = (it->close - peak) / peak;
            if (dd < mdd) mdd = dd;
        }
        last = it;
    }
    r.duration_days = time::diff_days(last->date, begin->date);
    r.total_return  = (last->close > 0 && p0 > 0) ? std::log(last->close / p0) : 0.0;
    r.max_drawdown  = mdd;
    return r;
}

} // namespace macro::engine
