#pragma once

#include "util/time_utils.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace macro {

// -----------------------------------------------------------------------------
// Asset universe
// -----------------------------------------------------------------------------

enum class AssetClass {
    Equity,
    ETF,
    Bond,
    Crypto,
    Commodity,
    FX,
    Index,
    Unknown,
};

const char* to_string(AssetClass c);
AssetClass parse_asset_class(const std::string& s);

struct Asset {
    std::string id;             // logical id ("aapl", "btc")
    std::string display_name;   // "Apple Inc."
    AssetClass  asset_class = AssetClass::Unknown;

    // Provider-specific identifiers. Only the relevant ones are populated.
    std::string yahoo_symbol;   // "AAPL", "SPY", "GC=F"
    std::string coingecko_id;   // "bitcoin", "ethereum"
    std::string fred_series;    // optional: for series-style assets
};

// -----------------------------------------------------------------------------
// Time series
// -----------------------------------------------------------------------------

struct PricePoint {
    time::Date date;
    double     close = 0.0;
};

using PriceSeries = std::vector<PricePoint>; // sorted ascending by date

double pct_change(double from, double to);

// -----------------------------------------------------------------------------
// Macro events
// -----------------------------------------------------------------------------

enum class EventKind {
    CPI,
    CorePCE,
    PPI,
    NonFarmPayrolls,
    UnemploymentRate,
    FOMC,            // FOMC rate decision
    GDP,
    RetailSales,
    ISM_Manufacturing,
    ConsumerSentiment,
    Unknown,
};

const char* to_string(EventKind k);
EventKind   parse_event_kind(const std::string& s);

struct HistoricalRelease {
    time::Date date;
    double     actual    = 0.0;
    double     consensus = 0.0; // NaN if unknown
    double     previous  = 0.0;
    // signed "surprise" = actual - consensus
    double surprise() const { return actual - consensus; }
};

struct MacroEvent {
    EventKind   kind = EventKind::Unknown;
    std::string label;      // human readable
    time::Date  date;       // scheduled release date (UTC)
    std::optional<double> consensus; // analyst expectation if known
    std::optional<double> previous;
    std::string source;     // "FRED release calendar" etc.
};

// -----------------------------------------------------------------------------
// Reaction analysis output
// -----------------------------------------------------------------------------

struct EventReaction {
    time::Date event_date;
    double     surprise = 0.0;       // actual - consensus (if available)
    double     return_1d = 0.0;      // log return D+1 vs D-1
    double     return_5d = 0.0;
    double     return_20d = 0.0;
};

struct DirectionalForecast {
    double mean_return = 0.0;
    double median_return = 0.0;
    double stddev = 0.0;
    double prob_up = 0.5;            // posterior P(return > 0)
    double prob_down = 0.5;
    double ci68_low = 0.0, ci68_high = 0.0;
    double ci95_low = 0.0, ci95_high = 0.0;
    double var95 = 0.0;              // 1-day 95% VaR (loss, positive number)
    double es95  = 0.0;              // 95% expected shortfall
    int    sample_size = 0;
    double sign_test_p = 1.0;        // two-sided p-value of sign test

    // EWMA-weighted variants (recent events count more)
    double weighted_prob_up = 0.5;
    double weighted_mean    = 0.0;
    double weighted_median  = 0.0;
    double ewma_half_life_samples = 24.0;

    // Regime-conditional forecast (NaN-friendly fields)
    int    regime_sample_size = 0;
    double regime_prob_up   = 0.5;
    double regime_mean      = 0.0;

    // Multiple-testing corrected p-values (filled by report layer)
    double bonferroni_p = 1.0;
    double bh_q         = 1.0;
};

struct BacktestSummary {
    int    n_events = 0;
    double brier = 0.0;
    double log_loss = 0.0;
    double hit_rate = 0.0;
    double pnl_total = 0.0;
    double pnl_sharpe = 0.0;
    double pnl_sortino = 0.0;
    double max_drawdown = 0.0;
};

struct ScenarioResult {
    MacroEvent event;
    int        horizon_days = 5;
    DirectionalForecast forecast;
    // Sensitivity slope: regression beta of return on surprise.
    std::optional<double> surprise_beta;
    // Standardised next-event surprise expectation (uses last consensus vs previous).
    std::optional<double> expected_surprise_z;
    // Detected regime at the upcoming event date.
    std::string regime = "Unknown";
    // GARCH-implied 1-day sigma (log-return scale).
    double garch_sigma_1d = 0.0;
    // Monte-Carlo projected price band.
    double mc_price_p05 = 0.0;
    double mc_price_p50 = 0.0;
    double mc_price_p95 = 0.0;
};

struct StressEntry {
    std::string scenario;
    double      total_log_return = 0.0;
    double      max_drawdown = 0.0;
    int         duration_days = 0;
};

struct AssetReport {
    Asset asset;
    double                spot_price = 0.0;
    double                hist_vol_annualized = 0.0;
    double                garch_vol_annualized = 0.0;
    std::vector<ScenarioResult> scenarios;
    std::map<std::string /*event*/, BacktestSummary> backtests;
    std::vector<StressEntry>    stresses;
};

struct PortfolioSnapshot {
    bool   present = false;
    int    n_assets = 0;
    int    horizon_days = 1;
    double mean_return = 0.0;
    double stddev = 0.0;
    double var95 = 0.0;
    double var99 = 0.0;
    double es95 = 0.0;
    double es99 = 0.0;
    std::vector<std::string> asset_ids;
    std::vector<double>      weights;
    std::vector<double>      component_var95;
    std::vector<double>      marginal_var95;
    std::vector<std::string> correl_labels;
    std::vector<std::vector<double>> correlation;
};

} // namespace macro
