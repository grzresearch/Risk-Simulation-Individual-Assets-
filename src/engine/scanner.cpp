#include "engine/scanner.h"

#include "engine/historical_analyzer.h"
#include "engine/monte_carlo.h"
#include "engine/risk_calculator.h"
#include "engine/statistical_engine.h"
#include "util/logger.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <string>

namespace macro::engine {

Scanner::Scanner(const Config& cfg,
                 data::FredClient& fred,
                 data::EconCalendar& cal,
                 data::PriceRepository& prices)
    : cfg_(cfg), fred_(fred), cal_(cal), prices_(prices) {}

ScenarioResult Scanner::build_scenario(const Asset& a,
                                       const MacroEvent& ev,
                                       const PriceSeries& history,
                                       const std::vector<HistoricalRelease>& releases,
                                       double spot,
                                       double daily_vol,
                                       const std::vector<double>& baseline_returns) {
    ScenarioResult sr;
    sr.event = ev;

    const int horizon = 5; // default 1-week reaction horizon
    sr.horizon_days = horizon;

    auto reactions = event_window_returns(history, releases, cfg_.return_horizons);
    auto sample    = returns_at_horizon(reactions, horizon);

    sr.forecast = forecast_from_sample(sample);
    sr.surprise_beta = surprise_sensitivity(reactions, horizon);

    // Monte Carlo: mixture of event-conditional shock + baseline drift
    double mu = 0.0;
    if (!baseline_returns.empty()) {
        for (double v : baseline_returns) mu += v;
        mu /= static_cast<double>(baseline_returns.size());
    }
    (void)daily_vol; // sigma is implicit in the baseline_returns sample

    if (spot > 0.0) {
        auto terminals = mixture_terminal_prices(
            spot, sample, baseline_returns, horizon, cfg_.mc_paths,
            /*weight_event=*/1.0, // use empirical event sample on day 1
            static_cast<unsigned>(std::hash<std::string>{}(a.id + ev.label)));
        sr.mc_price_p05 = stats::quantile(terminals, 0.05);
        sr.mc_price_p50 = stats::quantile(terminals, 0.50);
        sr.mc_price_p95 = stats::quantile(terminals, 0.95);
    }
    return sr;
}

std::vector<AssetReport> Scanner::run() {
    std::vector<AssetReport> out;

    auto today    = time::today_utc();
    auto end_date = time::add_days(today, cfg_.horizon_days);
    auto history_start = time::add_days(today, -365 * cfg_.history_years);

    const std::string from_iso  = time::to_iso(history_start);
    const std::string to_iso    = time::to_iso(today);
    const std::string scan_from = time::to_iso(today);
    const std::string scan_to   = time::to_iso(end_date);

    // 1. Fetch upcoming events once (shared across assets)
    auto upcoming = cal_.upcoming(cfg_.events, scan_from, scan_to);
    log::info("Upcoming events in next " + std::to_string(cfg_.horizon_days) +
              "d: " + std::to_string(upcoming.size()));

    // 2. Cache historical releases per event kind
    std::map<EventKind, std::vector<HistoricalRelease>> hist_releases;
    for (auto k : cfg_.events) {
        auto map = data::fred_mapping(k);
        if (map.series_id.empty() || !fred_.has_key()) continue;
        hist_releases[k] = fred_.fetch_release_history(map.series_id, from_iso, to_iso);
        log::info(std::string("Historical releases for ") + to_string(k) + ": " +
                  std::to_string(hist_releases[k].size()));
    }

    // 3. For each asset, fetch prices and build scenarios
    for (const auto& a : cfg_.assets) {
        AssetReport rep;
        rep.asset = a;
        const auto& prices = prices_.history(a, from_iso, to_iso);
        if (prices.size() < 60) {
            log::warn("Insufficient history for " + a.id + " (have " +
                      std::to_string(prices.size()) + " points)");
            out.push_back(rep);
            continue;
        }
        rep.spot_price = prices_.spot(a);
        if (rep.spot_price <= 0.0) rep.spot_price = prices.back().close;
        rep.hist_vol_annualized = annualized_vol(prices);

        auto baseline = log_returns(prices);
        double daily_sigma = rep.hist_vol_annualized / std::sqrt(252.0);

        for (const auto& ev : upcoming) {
            auto it = hist_releases.find(ev.kind);
            if (it == hist_releases.end() || it->second.empty()) continue;
            auto sc = build_scenario(a, ev, prices, it->second,
                                     rep.spot_price, daily_sigma, baseline);
            rep.scenarios.push_back(std::move(sc));
        }
        out.push_back(std::move(rep));
    }
    return out;
}

} // namespace macro::engine
