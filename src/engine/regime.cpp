#include "engine/regime.h"

#include <algorithm>

namespace macro::engine {

const char* to_string(Regime r) {
    switch (r) {
        case Regime::Easing:      return "Easing";
        case Regime::Neutral:     return "Neutral";
        case Regime::Hiking:      return "Hiking";
        case Regime::Stagflation: return "Stagflation";
        case Regime::Risk_Off:    return "Risk-Off";
        case Regime::Unknown:     return "Unknown";
    }
    return "Unknown";
}

Regime classify_regime(const RegimeContext& ctx) {
    // Priority-ordered rules.
    if (ctx.vix && *ctx.vix > 28.0) return Regime::Risk_Off;
    if (ctx.equity_dd_3m && *ctx.equity_dd_3m < -0.10) return Regime::Risk_Off;
    if (ctx.cpi_yoy && *ctx.cpi_yoy > 5.0 &&
        ctx.unemployment && *ctx.unemployment > 5.0) {
        return Regime::Stagflation;
    }
    if (ctx.cpi_yoy && *ctx.cpi_yoy > 3.0 &&
        ctx.fed_funds && *ctx.fed_funds > 2.5) {
        return Regime::Hiking;
    }
    if (ctx.fed_funds && *ctx.fed_funds < 1.0) return Regime::Easing;
    if (ctx.cpi_yoy && *ctx.cpi_yoy < 2.0) return Regime::Easing;
    if (ctx.cpi_yoy.has_value() || ctx.fed_funds.has_value()) return Regime::Neutral;
    return Regime::Unknown;
}

namespace {

std::string year_month(time::Date d) {
    auto s = time::to_iso(d);
    return s.substr(0, 7);
}

template <typename M>
std::optional<double> latest_at(const M& m, const std::string& ym) {
    if (m.empty()) return std::nullopt;
    auto it = m.upper_bound(ym);
    if (it == m.begin()) return std::nullopt;
    --it;
    return it->second;
}

} // namespace

RegimeHistory build_regime_history(
    const std::map<std::string, double>& cpi_yoy,
    const std::map<std::string, double>& fed_funds,
    const std::map<std::string, double>& unemployment,
    const std::map<std::string, double>& vix,
    const std::map<std::string, double>& spy_dd_3m,
    time::Date from, time::Date to) {
    RegimeHistory h;
    auto cur = from;
    while (cur <= to) {
        RegimeContext ctx;
        const std::string ym = year_month(cur);
        ctx.cpi_yoy      = latest_at(cpi_yoy,      ym);
        ctx.fed_funds    = latest_at(fed_funds,    ym);
        ctx.unemployment = latest_at(unemployment, ym);
        ctx.vix          = latest_at(vix,          ym);
        ctx.equity_dd_3m = latest_at(spy_dd_3m,    ym);
        h.dates.push_back(cur);
        h.regimes.push_back(classify_regime(ctx));
        cur = time::add_days(cur, 1);
    }
    return h;
}

Regime regime_at(const RegimeHistory& h, time::Date d) {
    if (h.dates.empty()) return Regime::Unknown;
    auto it = std::lower_bound(h.dates.begin(), h.dates.end(), d);
    if (it == h.dates.end()) return h.regimes.back();
    std::size_t i = static_cast<std::size_t>(it - h.dates.begin());
    if (*it > d && i > 0) --i;
    return h.regimes[i];
}

std::vector<double>
filter_by_regime(const std::vector<time::Date>& dates,
                 const std::vector<double>& values,
                 const RegimeHistory& h,
                 Regime target) {
    std::vector<double> out;
    out.reserve(values.size());
    for (std::size_t i = 0; i < std::min(dates.size(), values.size()); ++i) {
        if (regime_at(h, dates[i]) == target) out.push_back(values[i]);
    }
    return out;
}

} // namespace macro::engine
