#pragma once

#include "models.h"

#include <string>
#include <vector>

namespace macro {

struct ApiKeys {
    std::string fred;        // FRED API key (https://fred.stlouisfed.org/docs/api/api_key.html)
    std::string alphavantage; // optional fallback for prices
};

struct PortfolioHoldingCfg {
    std::string asset_id;
    double      weight = 0.0;
};

struct Config {
    ApiKeys            api_keys;
    std::vector<Asset> assets;
    std::vector<EventKind> events;     // events the user wants screened
    int                horizon_days = 90; // upcoming-events lookahead
    std::vector<int>   return_horizons = {1, 5, 20}; // D+N returns to study
    int                history_years = 12; // how far back to load history
    int                mc_paths = 10000;
    bool               offline = false; // skip network, use cached/json
    std::string        cache_dir = ".cache";

    // Phase 2 knobs
    double             ewma_half_life_samples = 24.0;
    double             fdr_alpha = 0.05;
    bool               enable_regime_conditioning = true;
    bool               enable_garch = true;

    // Phase 3 portfolio
    std::vector<PortfolioHoldingCfg> portfolio;
    bool               enable_portfolio_risk = true;
    bool               enable_stress_tests = true;

    // Phase 4 backtest
    bool               enable_backtest = true;
    int                backtest_min_train = 24;
};

Config load_config(const std::string& path);
void   write_default_config(const std::string& path);

} // namespace macro
