#include "engine/scanner.h"

#include "engine/backtest.h"
#include "engine/calibration.h"
#include "engine/garch.h"
#include "engine/historical_analyzer.h"
#include "engine/monte_carlo.h"
#include "engine/multitest.h"
#include "engine/portfolio_risk.h"
#include "engine/regime.h"
#include "engine/risk_calculator.h"
#include "engine/statistical_engine.h"
#include "engine/stress.h"
#include "engine/surprise.h"
#include "util/logger.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <string>
#include <unordered_map>

namespace macro::engine {

Scanner::Scanner(const Config& cfg,
                 data::FredClient& fred,
                 data::EconCalendar& cal,
                 data::PriceRepository& prices,
                 data::ConsensusProvider& consensus)
    : cfg_(cfg), fred_(fred), cal_(cal), prices_(prices), consensus_(consensus) {}

namespace {

// Build a year-month -> latest value map from a release history.
std::map<std::string, double> ym_map(const std::vector<HistoricalRelease>& releases) {
    std::map<std::string, double> out;
    for (const auto& r : releases) {
        std::string ym = time::to_iso(r.date).substr(0, 7);
        out[ym] = r.actual;
    }
    return out;
}

std::map<std::string, double> ym_yoy(const std::vector<HistoricalRelease>& releases) {
    std::map<std::string, double> out;
    if (releases.size() < 13) return out;
    for (std::size_t i = 12; i < releases.size(); ++i) {
        double prev = releases[i - 12].actual;
        double cur  = releases[i].actual;
        if (prev <= 0.0) continue;
        std::string ym = time::to_iso(releases[i].date).substr(0, 7);
        out[ym] = 100.0 * (cur / prev - 1.0);
    }
    return out;
}

std::map<std::string, double> dd_3m_from_prices(const PriceSeries& p) {
    std::map<std::string, double> out;
    if (p.size() < 70) return out;
    for (std::size_t i = 63; i < p.size(); ++i) {
        double peak = 0.0;
        for (std::size_t j = i - 63; j <= i; ++j) peak = std::max(peak, p[j].close);
        double dd = (peak > 0) ? (p[i].close - peak) / peak : 0.0;
        out[time::to_iso(p[i].date).substr(0, 7)] = dd;
    }
    return out;
}

} // namespace

ScenarioResult Scanner::build_scenario(const Asset& a,
                                       const MacroEvent& ev,
                                       const PriceSeries& history,
                                       const std::vector<HistoricalRelease>& releases,
                                       double spot,
                                       double garch_sigma_1d,
                                       const std::vector<double>& baseline_returns,
                                       const std::vector<double>& regime_filtered_returns,
                                       const std::string& current_regime) {
    ScenarioResult sr;
    sr.event = ev;
    sr.horizon_days = 5;
    sr.regime = current_regime;
    sr.garch_sigma_1d = garch_sigma_1d;

    auto reactions = event_window_returns(history, releases, cfg_.return_horizons);
    auto sample    = returns_at_horizon(reactions, sr.horizon_days);

    sr.forecast = forecast_from_sample(sample);
    sr.forecast.ewma_half_life_samples = cfg_.ewma_half_life_samples;
    sr.surprise_beta = surprise_sensitivity(reactions, sr.horizon_days);

    // Regime-conditional sub-sample
    if (cfg_.enable_regime_conditioning && !regime_filtered_returns.empty()) {
        sr.forecast.regime_sample_size = static_cast<int>(regime_filtered_returns.size());
        sr.forecast.regime_prob_up = stats::posterior_prob_up(regime_filtered_returns);
        auto rs = stats::summarize(regime_filtered_returns);
        sr.forecast.regime_mean = rs.mean;
    }

    // Standardised expected surprise from latest consensus/previous
    auto cs = consensus_.consensus(ev.kind, ev.date);
    auto pv = consensus_.previous (ev.kind, ev.date);
    if (cs && pv) {
        double raw = *cs - *pv;
        double z   = standardise_one(releases, ev.date, raw);
        if (!std::isnan(z)) sr.expected_surprise_z = z;
    }

    if (spot > 0.0) {
        auto terminals = mixture_terminal_prices(
            spot, sample, baseline_returns, sr.horizon_days, cfg_.mc_paths, 1.0,
            static_cast<unsigned>(std::hash<std::string>{}(a.id + ev.label + time::to_iso(ev.date))));
        sr.mc_price_p05 = stats::quantile(terminals, 0.05);
        sr.mc_price_p50 = stats::quantile(terminals, 0.50);
        sr.mc_price_p95 = stats::quantile(terminals, 0.95);
    }
    return sr;
}

void Scanner::apply_multi_testing(std::vector<AssetReport>& reports) {
    std::vector<TestRow> rows;
    rows.reserve(64);
    std::vector<std::pair<std::size_t, std::size_t>> coords; // (asset_idx, scenario_idx)
    for (std::size_t i = 0; i < reports.size(); ++i) {
        for (std::size_t k = 0; k < reports[i].scenarios.size(); ++k) {
            TestRow row;
            row.label = reports[i].asset.id + ":" + to_string(reports[i].scenarios[k].event.kind);
            row.p_value = reports[i].scenarios[k].forecast.sign_test_p;
            rows.push_back(row);
            coords.emplace_back(i, k);
        }
    }
    auto adj = apply_corrections(std::move(rows), cfg_.fdr_alpha);
    for (std::size_t t = 0; t < adj.size(); ++t) {
        auto [ai, si] = coords[t];
        reports[ai].scenarios[si].forecast.bonferroni_p = adj[t].bonferroni;
        reports[ai].scenarios[si].forecast.bh_q         = adj[t].bh_q;
    }
}

PortfolioSnapshot Scanner::build_portfolio_snapshot(
    const std::vector<AssetReport>& reports,
    const std::vector<Asset>& assets,
    std::map<std::string, std::vector<double>>& asset_returns_aligned) {
    PortfolioSnapshot ps;
    if (cfg_.portfolio.empty() || !cfg_.enable_portfolio_risk) return ps;
    (void)reports;

    // Align by intersecting common dates is too expensive here; use truncation
    // to the min-length tail (already business-daily and roughly aligned).
    std::vector<std::string> ids;
    std::vector<std::vector<double>> rets;
    for (const auto& a : assets) {
        auto it = asset_returns_aligned.find(a.id);
        if (it == asset_returns_aligned.end() || it->second.size() < 60) continue;
        ids.push_back(a.id);
        rets.push_back(it->second);
    }
    if (ids.empty()) return ps;

    std::size_t min_len = rets.front().size();
    for (const auto& r : rets) min_len = std::min(min_len, r.size());
    for (auto& r : rets) r.erase(r.begin(), r.begin() + (r.size() - min_len));

    std::vector<PortfolioHolding> hold;
    for (const auto& pc : cfg_.portfolio) hold.push_back({pc.asset_id, pc.weight});

    auto pr = portfolio_risk(hold, ids, rets, /*horizon_days*/1, /*mc_paths*/30000, 17u);

    ps.present     = true;
    ps.n_assets    = pr.n_assets;
    ps.horizon_days = static_cast<int>(pr.horizon_days);
    ps.mean_return = pr.mean_return;
    ps.stddev      = pr.stddev;
    ps.var95       = pr.var95;
    ps.var99       = pr.var99;
    ps.es95        = pr.es95;
    ps.es99        = pr.es99;
    ps.asset_ids   = pr.asset_ids;
    ps.weights     = pr.weights;
    ps.component_var95 = pr.component_var95;
    ps.marginal_var95  = pr.marginal_var95;
    ps.correl_labels   = pr.correl_labels;
    ps.correlation     = pr.correlation;
    return ps;
}

ScanOutput Scanner::run() {
    ScanOutput out;

    auto today    = time::today_utc();
    auto end_date = time::add_days(today, cfg_.horizon_days);
    auto history_start = time::add_days(today, -365 * cfg_.history_years);

    const std::string from_iso  = time::to_iso(history_start);
    const std::string to_iso    = time::to_iso(today);
    const std::string scan_from = time::to_iso(today);
    const std::string scan_to   = time::to_iso(end_date);

    auto upcoming = cal_.upcoming(cfg_.events, scan_from, scan_to);
    log::info("Upcoming events in next " + std::to_string(cfg_.horizon_days) +
              "d: " + std::to_string(upcoming.size()));

    // Historical releases per event kind, enriched with consensus
    std::map<EventKind, std::vector<HistoricalRelease>> hist_releases;
    for (auto k : cfg_.events) {
        auto map = data::fred_mapping(k);
        if (map.series_id.empty() || !fred_.has_key()) continue;
        auto hist = fred_.fetch_release_history(map.series_id, from_iso, to_iso);
        consensus_.enrich(k, hist);
        hist_releases[k] = std::move(hist);
        log::info(std::string("Historical releases for ") + to_string(k) + ": " +
                  std::to_string(hist_releases[k].size()));
    }

    // Build a regime history using CPI YoY + FedFunds + Unemployment
    RegimeHistory regime_h;
    if (cfg_.enable_regime_conditioning) {
        std::map<std::string, double> cpi_yoy, ffr, unemp;
        if (hist_releases.count(EventKind::CPI))               cpi_yoy = ym_yoy(hist_releases[EventKind::CPI]);
        if (hist_releases.count(EventKind::FOMC))              ffr     = ym_map(hist_releases[EventKind::FOMC]);
        if (hist_releases.count(EventKind::UnemploymentRate))  unemp   = ym_map(hist_releases[EventKind::UnemploymentRate]);
        std::map<std::string, double> vix; // not currently fetched
        std::map<std::string, double> spy_dd;
        // Use SPY series if available among assets to feed drawdown context.
        for (const auto& a : cfg_.assets) {
            if (a.id == "spy" || a.yahoo_symbol == "SPY") {
                const auto& p = prices_.history(a, from_iso, to_iso);
                spy_dd = dd_3m_from_prices(p);
                break;
            }
        }
        regime_h = build_regime_history(cpi_yoy, ffr, unemp, vix, spy_dd,
                                        history_start, end_date);
    }
    std::string current_regime_str = to_string(regime_at(regime_h, today));

    std::map<std::string, std::vector<double>> per_asset_returns;

    for (const auto& a : cfg_.assets) {
        AssetReport rep;
        rep.asset = a;
        const auto& prices = prices_.history(a, from_iso, to_iso);
        if (prices.size() < 60) {
            log::warn("Insufficient history for " + a.id + " (have " +
                      std::to_string(prices.size()) + " points)");
            out.assets.push_back(rep);
            continue;
        }
        rep.spot_price = prices_.spot(a);
        if (rep.spot_price <= 0.0) rep.spot_price = prices.back().close;
        rep.hist_vol_annualized = annualized_vol(prices);

        auto baseline = log_returns(prices);
        per_asset_returns[a.id] = baseline;

        double garch_sigma = 0.0;
        if (cfg_.enable_garch) {
            auto gp = fit_garch11(baseline);
            garch_sigma = forecast_sigma_one_step(gp, baseline);
            rep.garch_vol_annualized = annualise(garch_sigma);
        }

        for (const auto& ev : upcoming) {
            auto it = hist_releases.find(ev.kind);
            if (it == hist_releases.end() || it->second.empty()) continue;
            std::vector<double> regime_filtered;
            if (cfg_.enable_regime_conditioning && !regime_h.dates.empty()) {
                auto reactions = event_window_returns(prices, it->second, cfg_.return_horizons);
                Regime curr = regime_at(regime_h, today);
                std::vector<time::Date> dates;
                std::vector<double> returns;
                for (const auto& r : reactions) {
                    dates.push_back(r.event_date);
                    returns.push_back(r.return_5d);
                }
                regime_filtered = filter_by_regime(dates, returns, regime_h, curr);
            }
            rep.scenarios.push_back(
                build_scenario(a, ev, prices, it->second, rep.spot_price,
                               garch_sigma, baseline, regime_filtered,
                               current_regime_str));
        }

        // Backtests per event kind that has enough history
        if (cfg_.enable_backtest) {
            for (auto& [k, releases] : hist_releases) {
                if (releases.empty()) continue;
                auto bt = walk_forward(to_string(k), prices, releases, 5,
                                       cfg_.backtest_min_train);
                if (bt.n_events == 0) continue;
                BacktestSummary s;
                s.n_events     = bt.n_events;
                s.brier        = bt.calib.brier;
                s.log_loss     = bt.calib.log_loss;
                s.hit_rate     = bt.calib.hit_rate;
                s.pnl_total    = bt.pnl_total;
                s.pnl_sharpe   = bt.pnl_sharpe;
                s.pnl_sortino  = bt.pnl_sortino;
                s.max_drawdown = bt.max_drawdown;
                rep.backtests.emplace(to_string(k), s);
            }
        }

        // Stress replays
        if (cfg_.enable_stress_tests) {
            for (const auto& sc : default_scenarios()) {
                auto sr = replay(sc, a.id, prices);
                if (sr.duration_days == 0) continue;
                StressEntry e;
                e.scenario        = sc.label;
                e.total_log_return = sr.total_return;
                e.max_drawdown    = sr.max_drawdown;
                e.duration_days   = sr.duration_days;
                rep.stresses.push_back(e);
            }
        }

        out.assets.push_back(std::move(rep));
    }

    apply_multi_testing(out.assets);
    out.portfolio = build_portfolio_snapshot(out.assets, cfg_.assets, per_asset_returns);
    return out;
}

} // namespace macro::engine
