#include "report/html_reporter.h"

#include "util/time_utils.h"
#include "util/version.h"

#include <iomanip>
#include <ostream>
#include <sstream>

namespace macro::report {

namespace {

std::string pct(double v) {
    std::ostringstream o; o << std::fixed << std::setprecision(2) << (v * 100.0) << "%";
    return o.str();
}
std::string sgnpct(double v) {
    std::ostringstream o;
    o << (v >= 0 ? "+" : "") << std::fixed << std::setprecision(2) << (v * 100.0) << "%";
    return o.str();
}
std::string num(double v, int p = 2) {
    std::ostringstream o; o << std::fixed << std::setprecision(p) << v;
    return o.str();
}

void write_header(std::ostream& o) {
    o << "<!doctype html><html><head><meta charset='utf-8'>"
      << "<title>macro_risk report</title>"
      << "<style>"
      << "body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Arial,sans-serif;"
         "background:#0e1015;color:#e6e9ef;margin:1.5rem;}"
      << "h1{font-size:1.4rem;margin:0 0 .25rem 0;}"
      << "h2{font-size:1.1rem;margin-top:1.5rem;color:#a4b0c2;}"
      << "table{border-collapse:collapse;width:100%;margin:.5rem 0;font-size:.85rem;}"
      << "th,td{padding:.35rem .55rem;border-bottom:1px solid #2a2f3a;text-align:right;white-space:nowrap;}"
      << "th{background:#1a1f2c;color:#9fb0c8;text-align:right;font-weight:600;}"
      << "th:first-child,td:first-child{text-align:left;}"
      << ".pos{color:#5bd99a;} .neg{color:#ff7a8a;} .muted{color:#8590a6;}"
      << ".chip{display:inline-block;padding:.05rem .45rem;border-radius:.6rem;"
         "background:#2a2f3a;color:#9fb0c8;font-size:.75rem;}"
      << ".kpi{display:inline-block;margin-right:1rem;}"
      << "</style></head><body>";
}

void write_scenario_table(std::ostream& o, const AssetReport& r) {
    o << "<table><thead><tr>"
      << "<th>Date</th><th>Event</th><th>Regime</th>"
      << "<th>P&uarr;</th><th>P&uarr; (w)</th><th>P&uarr; (R)</th>"
      << "<th>Mean</th><th>95% CI low</th><th>95% CI high</th>"
      << "<th>VaR95</th><th>BH-q</th><th>n</th>"
      << "<th>MC p05</th><th>MC p50</th><th>MC p95</th>"
      << "</tr></thead><tbody>";
    for (const auto& s : r.scenarios) {
        const auto& f = s.forecast;
        std::string mc_class = (f.mean_return >= 0 ? "pos" : "neg");
        o << "<tr><td>" << time::to_iso(s.event.date) << "</td>"
          << "<td>" << to_string(s.event.kind) << "</td>"
          << "<td><span class='chip'>" << s.regime << "</span></td>"
          << "<td>" << pct(f.prob_up) << "</td>"
          << "<td>" << pct(f.weighted_prob_up) << "</td>"
          << "<td>" << pct(f.regime_prob_up) << "</td>"
          << "<td class='" << mc_class << "'>" << sgnpct(f.mean_return) << "</td>"
          << "<td>" << sgnpct(f.ci95_low) << "</td>"
          << "<td>" << sgnpct(f.ci95_high) << "</td>"
          << "<td>" << pct(f.var95) << "</td>"
          << "<td>" << num(f.bh_q, 3) << "</td>"
          << "<td class='muted'>" << f.sample_size << "</td>"
          << "<td>" << num(s.mc_price_p05, 2) << "</td>"
          << "<td>" << num(s.mc_price_p50, 2) << "</td>"
          << "<td>" << num(s.mc_price_p95, 2) << "</td>"
          << "</tr>";
    }
    o << "</tbody></table>";
}

} // namespace

void to_html(std::ostream& out, const engine::ScanOutput& result) {
    write_header(out);
    out << "<h1>macro_risk &mdash; direction &amp; risk forecast</h1>"
        << "<div class='muted'>run " << time::to_iso(time::today_utc())
        << " &middot; " << version_string() << "</div>";
    for (const auto& r : result.assets) {
        if (r.scenarios.empty()) continue;
        out << "<h2>" << r.asset.id << " &mdash; " << r.asset.display_name
            << " <span class='chip'>" << to_string(r.asset.asset_class) << "</span></h2>";
        out << "<div>"
            << "<span class='kpi'>spot " << num(r.spot_price, 4) << "</span>"
            << "<span class='kpi'>hist vol " << pct(r.hist_vol_annualized) << "</span>"
            << "<span class='kpi'>GARCH vol " << pct(r.garch_vol_annualized) << "</span>"
            << "</div>";
        write_scenario_table(out, r);
        if (!r.backtests.empty()) {
            out << "<table><thead><tr><th>Event</th><th>N</th><th>Brier</th>"
                << "<th>LogLoss</th><th>Hit</th><th>PnL</th><th>Sharpe</th>"
                << "<th>MaxDD</th></tr></thead><tbody>";
            for (const auto& [name, bt] : r.backtests) {
                out << "<tr><td>" << name << "</td>"
                    << "<td>" << bt.n_events << "</td>"
                    << "<td>" << num(bt.brier, 3) << "</td>"
                    << "<td>" << num(bt.log_loss, 3) << "</td>"
                    << "<td>" << pct(bt.hit_rate) << "</td>"
                    << "<td>" << sgnpct(bt.pnl_total) << "</td>"
                    << "<td>" << num(bt.pnl_sharpe, 2) << "</td>"
                    << "<td>" << sgnpct(bt.max_drawdown) << "</td></tr>";
            }
            out << "</tbody></table>";
        }
    }
    if (result.portfolio.present) {
        const auto& p = result.portfolio;
        out << "<h2>Portfolio risk (1d)</h2><div>"
            << "<span class='kpi'>Mean " << sgnpct(p.mean_return) << "</span>"
            << "<span class='kpi'>StdDev " << pct(p.stddev) << "</span>"
            << "<span class='kpi'>VaR95 " << pct(p.var95) << "</span>"
            << "<span class='kpi'>VaR99 " << pct(p.var99) << "</span>"
            << "<span class='kpi'>ES95 "  << pct(p.es95)  << "</span>"
            << "<span class='kpi'>ES99 "  << pct(p.es99)  << "</span></div>";
    }
    out << "</body></html>";
}

} // namespace macro::report
