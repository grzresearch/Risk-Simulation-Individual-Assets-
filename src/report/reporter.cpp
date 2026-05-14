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
std::string opt(const std::optional<double>& v, int prec = 4) {
    return v ? fmt(*v, prec) : "-";
}

} // namespace

void print_text(std::ostream& out, const engine::ScanOutput& result) {
    out << "============================================================\n";
    out << "  MACRO RISK ENGINE - Direction & Risk Forecast\n";
    out << "  Run date: " << time::to_iso(time::today_utc()) << "\n";
    out << "============================================================\n\n";

    for (const auto& r : result.assets) {
        out << "-- " << r.asset.id << " | " << r.asset.display_name
            << " (" << to_string(r.asset.asset_class) << ")\n";
        out << "   Spot: " << fmt(r.spot_price, 4)
            << "  |  hist vol: " << pct(r.hist_vol_annualized)
            << "  |  GARCH vol: " << pct(r.garch_vol_annualized) << "\n";

        if (r.scenarios.empty()) {
            out << "   (no upcoming events with matched history)\n\n";
            continue;
        }
        out << "   Scenarios (5d horizon):\n";
        out << "   " << std::left
            << std::setw(12) << "Date"
            << std::setw(22) << "Event"
            << std::setw(8)  << "Pup"
            << std::setw(8)  << "Pup_w"
            << std::setw(8)  << "Pup_R"
            << std::setw(10) << "Mean"
            << std::setw(18) << "95% CI"
            << std::setw(8)  << "VaR95"
            << std::setw(8)  << "BH-q"
            << std::setw(9)  << "Sz"
            << "Regime\n";

        for (const auto& s : r.scenarios) {
            const auto& f = s.forecast;
            std::ostringstream ci; ci << "[" << sgnpct(f.ci95_low) << "," << sgnpct(f.ci95_high) << "]";
            out << "   "
                << std::left
                << std::setw(12) << time::to_iso(s.event.date)
                << std::setw(22) << to_string(s.event.kind)
                << std::setw(8)  << pct(f.prob_up)
                << std::setw(8)  << pct(f.weighted_prob_up)
                << std::setw(8)  << pct(f.regime_prob_up)
                << std::setw(10) << sgnpct(f.mean_return)
                << std::setw(18) << ci.str()
                << std::setw(8)  << pct(f.var95)
                << std::setw(8)  << fmt(f.bh_q, 3)
                << std::setw(9)  << opt(s.expected_surprise_z, 2)
                << s.regime << "\n";
            out << "       MC[" << s.horizon_days << "d]: "
                << "p05=" << fmt(s.mc_price_p05, 4)
                << "  p50=" << fmt(s.mc_price_p50, 4)
                << "  p95=" << fmt(s.mc_price_p95, 4)
                << "  beta(surprise)=" << (s.surprise_beta ? fmt(*s.surprise_beta, 4) : std::string("-"))
                << "  n=" << f.sample_size
                << " (regime n=" << f.regime_sample_size << ")\n";
        }

        if (!r.backtests.empty()) {
            out << "   Walk-forward backtest:\n";
            out << "       " << std::left
                << std::setw(22) << "Event"
                << std::setw(6)  << "N"
                << std::setw(9)  << "Brier"
                << std::setw(9)  << "LogLoss"
                << std::setw(8)  << "Hit"
                << std::setw(10) << "PnL"
                << std::setw(9)  << "Sharpe"
                << std::setw(9)  << "MaxDD"
                << "\n";
            for (const auto& [name, bt] : r.backtests) {
                out << "       " << std::left
                    << std::setw(22) << name
                    << std::setw(6)  << bt.n_events
                    << std::setw(9)  << fmt(bt.brier, 3)
                    << std::setw(9)  << fmt(bt.log_loss, 3)
                    << std::setw(8)  << pct(bt.hit_rate)
                    << std::setw(10) << sgnpct(bt.pnl_total)
                    << std::setw(9)  << fmt(bt.pnl_sharpe, 2)
                    << std::setw(9)  << sgnpct(bt.max_drawdown)
                    << "\n";
            }
        }
        if (!r.stresses.empty()) {
            out << "   Stress replays:\n";
            for (const auto& s : r.stresses) {
                out << "       " << std::left << std::setw(24) << s.scenario
                    << "ret=" << sgnpct(s.total_log_return)
                    << "  MaxDD=" << sgnpct(s.max_drawdown)
                    << "  " << s.duration_days << "d\n";
            }
        }
        out << "\n";
    }

    if (result.portfolio.present) {
        const auto& p = result.portfolio;
        out << "Portfolio (1d horizon):\n";
        out << "  Mean=" << sgnpct(p.mean_return)
            << "  Stdev=" << pct(p.stddev)
            << "  VaR95=" << pct(p.var95)
            << "  VaR99=" << pct(p.var99)
            << "  ES95="  << pct(p.es95)
            << "  ES99="  << pct(p.es99) << "\n";
        out << "  Component-VaR contribution:\n";
        for (std::size_t i = 0; i < p.asset_ids.size(); ++i) {
            if (std::fabs(p.weights[i]) < 1e-9) continue;
            out << "    " << std::left << std::setw(10) << p.asset_ids[i]
                << "w=" << fmt(p.weights[i], 3)
                << "  marg=" << fmt(p.marginal_var95[i], 4)
                << "  contrib=" << fmt(p.component_var95[i], 4) << "\n";
        }
    }

    out << "\nLegend: Pup empirical, Pup_w EWMA-weighted, Pup_R regime-conditional,\n"
        << "BH-q = Benjamini-Hochberg FDR-adjusted sign-test, MC band uses event\n"
        << "conditional shock + baseline empirical drift. GARCH(1,1) annualised\n"
        << "vol. Stress replays show actual historical paths of the same asset.\n";
}

std::string to_json(const engine::ScanOutput& result, bool pretty) {
    json root;
    root["run_date"] = time::to_iso(time::today_utc());
    json assets = json::array();
    for (const auto& r : result.assets) {
        json a;
        a["asset"] = {
            {"id", r.asset.id},
            {"name", r.asset.display_name},
            {"class", to_string(r.asset.asset_class)},
        };
        a["spot"] = r.spot_price;
        a["hist_vol_annualized"]  = r.hist_vol_annualized;
        a["garch_vol_annualized"] = r.garch_vol_annualized;

        a["scenarios"] = json::array();
        for (const auto& s : r.scenarios) {
            json sj;
            sj["event"] = {
                {"date", time::to_iso(s.event.date)},
                {"kind", to_string(s.event.kind)},
                {"source", s.event.source},
            };
            sj["horizon_days"] = s.horizon_days;
            sj["regime"]       = s.regime;
            sj["garch_sigma_1d"] = s.garch_sigma_1d;
            const auto& f = s.forecast;
            sj["forecast"] = {
                {"prob_up", f.prob_up},
                {"prob_down", f.prob_down},
                {"weighted_prob_up", f.weighted_prob_up},
                {"regime_prob_up",  f.regime_prob_up},
                {"regime_n",        f.regime_sample_size},
                {"mean_return", f.mean_return},
                {"weighted_mean", f.weighted_mean},
                {"median_return", f.median_return},
                {"stddev", f.stddev},
                {"ci68", {f.ci68_low, f.ci68_high}},
                {"ci95", {f.ci95_low, f.ci95_high}},
                {"var95", f.var95},
                {"es95",  f.es95},
                {"sample_size", f.sample_size},
                {"sign_test_p", f.sign_test_p},
                {"bonferroni_p", f.bonferroni_p},
                {"bh_q", f.bh_q},
            };
            if (s.surprise_beta)        sj["surprise_beta"] = *s.surprise_beta;
            if (s.expected_surprise_z)  sj["expected_surprise_z"] = *s.expected_surprise_z;
            sj["mc_band"] = {{"p05", s.mc_price_p05},{"p50", s.mc_price_p50},{"p95", s.mc_price_p95}};
            a["scenarios"].push_back(sj);
        }

        a["backtests"] = json::object();
        for (const auto& [name, bt] : r.backtests) {
            a["backtests"][name] = {
                {"n_events", bt.n_events},
                {"brier", bt.brier},
                {"log_loss", bt.log_loss},
                {"hit_rate", bt.hit_rate},
                {"pnl_total", bt.pnl_total},
                {"pnl_sharpe", bt.pnl_sharpe},
                {"pnl_sortino", bt.pnl_sortino},
                {"max_drawdown", bt.max_drawdown},
            };
        }
        a["stress"] = json::array();
        for (const auto& s : r.stresses) {
            a["stress"].push_back({
                {"scenario", s.scenario},
                {"total_log_return", s.total_log_return},
                {"max_drawdown", s.max_drawdown},
                {"duration_days", s.duration_days},
            });
        }
        assets.push_back(a);
    }
    root["assets"] = assets;

    if (result.portfolio.present) {
        const auto& p = result.portfolio;
        root["portfolio"] = {
            {"n_assets", p.n_assets},
            {"horizon_days", p.horizon_days},
            {"mean_return", p.mean_return},
            {"stddev", p.stddev},
            {"var95", p.var95},
            {"var99", p.var99},
            {"es95",  p.es95},
            {"es99",  p.es99},
            {"asset_ids", p.asset_ids},
            {"weights", p.weights},
            {"component_var95", p.component_var95},
            {"marginal_var95",  p.marginal_var95},
            {"correlation_labels", p.correl_labels},
            {"correlation", p.correlation},
        };
    }
    return pretty ? root.dump(2) : root.dump();
}

} // namespace macro::report
