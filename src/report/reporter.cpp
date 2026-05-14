#include "report/reporter.h"

#include "util/time_utils.h"

#include <nlohmann/json.hpp>

#include <iomanip>
#include <iostream>
#include <sstream>

namespace macro::report {

using nlohmann::json;

namespace {
std::string pct(double v) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << (v * 100.0) << "%";
    return oss.str();
}
std::string sgnpct(double v) {
    std::ostringstream oss;
    oss << (v >= 0 ? "+" : "") << std::fixed << std::setprecision(2) << (v * 100.0) << "%";
    return oss.str();
}
std::string fmt(double v, int prec = 2) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(prec) << v;
    return oss.str();
}
} // namespace

void print_text(std::ostream& out, const std::vector<AssetReport>& reports) {
    out << "============================================================\n";
    out << "  MACRO RISK ENGINE - Direction & Risk Forecast\n";
    out << "  Run date: " << time::to_iso(time::today_utc()) << "\n";
    out << "============================================================\n\n";

    for (const auto& r : reports) {
        out << "-- " << r.asset.id << " | " << r.asset.display_name
            << " (" << to_string(r.asset.asset_class) << ")\n";
        out << "   Spot: " << fmt(r.spot_price, 4)
            << "  |  Annualized vol: " << pct(r.hist_vol_annualized) << "\n";

        if (r.scenarios.empty()) {
            out << "   (no upcoming events within window or no historical sample)\n\n";
            continue;
        }
        out << "   Scenarios (5d horizon):\n";
        out << "   " << std::left
            << std::setw(12) << "Date"
            << std::setw(22) << "Event"
            << std::setw(9)  << "Pup"
            << std::setw(11) << "MeanRet"
            << std::setw(11) << "Median"
            << std::setw(20) << "95% CI"
            << std::setw(9)  << "VaR95"
            << std::setw(7)  << "n"
            << std::setw(9)  << "p(sgn)"
            << "Beta(surprise)\n";

        for (const auto& s : r.scenarios) {
            const auto& f = s.forecast;
            std::ostringstream ci; ci << "[" << sgnpct(f.ci95_low) << ", " << sgnpct(f.ci95_high) << "]";
            out << "   "
                << std::left
                << std::setw(12) << time::to_iso(s.event.date)
                << std::setw(22) << to_string(s.event.kind)
                << std::setw(9)  << pct(f.prob_up)
                << std::setw(11) << sgnpct(f.mean_return)
                << std::setw(11) << sgnpct(f.median_return)
                << std::setw(20) << ci.str()
                << std::setw(9)  << pct(f.var95)
                << std::setw(7)  << f.sample_size
                << std::setw(9)  << fmt(f.sign_test_p, 3)
                << (s.surprise_beta ? fmt(*s.surprise_beta, 4) : std::string("-"))
                << "\n";
            out << "       MC band (D+" << s.horizon_days << "): "
                << "p05=" << fmt(s.mc_price_p05, 4) << "  "
                << "p50=" << fmt(s.mc_price_p50, 4) << "  "
                << "p95=" << fmt(s.mc_price_p95, 4) << "\n";
        }
        out << "\n";
    }
    out << "Legend: Pup = posterior P(return>0), Beta = OLS slope of return on\n"
        << "surprise, p(sgn) = two-sided sign-test p-value. MC band uses event-\n"
        << "conditional shock on day 1 then baseline empirical returns.\n";
}

std::string to_json(const std::vector<AssetReport>& reports, bool pretty) {
    json j = json::array();
    for (const auto& r : reports) {
        json a;
        a["asset"] = {
            {"id", r.asset.id},
            {"name", r.asset.display_name},
            {"class", to_string(r.asset.asset_class)},
        };
        a["spot"] = r.spot_price;
        a["hist_vol_annualized"] = r.hist_vol_annualized;
        a["scenarios"] = json::array();
        for (const auto& s : r.scenarios) {
            json sj;
            sj["event"] = {
                {"date", time::to_iso(s.event.date)},
                {"kind", to_string(s.event.kind)},
                {"label", s.event.label},
                {"source", s.event.source},
            };
            sj["horizon_days"] = s.horizon_days;
            sj["forecast"] = {
                {"mean_return",    s.forecast.mean_return},
                {"median_return",  s.forecast.median_return},
                {"stddev",         s.forecast.stddev},
                {"prob_up",        s.forecast.prob_up},
                {"prob_down",      s.forecast.prob_down},
                {"ci68",           {s.forecast.ci68_low, s.forecast.ci68_high}},
                {"ci95",           {s.forecast.ci95_low, s.forecast.ci95_high}},
                {"var95",          s.forecast.var95},
                {"es95",           s.forecast.es95},
                {"sample_size",    s.forecast.sample_size},
                {"sign_test_p",    s.forecast.sign_test_p},
            };
            if (s.surprise_beta) sj["surprise_beta"] = *s.surprise_beta;
            sj["mc_band"] = {
                {"p05", s.mc_price_p05},
                {"p50", s.mc_price_p50},
                {"p95", s.mc_price_p95},
            };
            a["scenarios"].push_back(sj);
        }
        j.push_back(a);
    }
    return pretty ? j.dump(2) : j.dump();
}

} // namespace macro::report
