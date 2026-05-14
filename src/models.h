#pragma once

#include "util/time_utils.h"

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
};

struct ScenarioResult {
    MacroEvent event;
    int        horizon_days = 5;
    DirectionalForecast forecast;
    // Sensitivity slope: regression beta of return on surprise.
    std::optional<double> surprise_beta;
    // Monte-Carlo projected price band.
    double mc_price_p05 = 0.0;
    double mc_price_p50 = 0.0;
    double mc_price_p95 = 0.0;
};

struct AssetReport {
    Asset asset;
    double                spot_price = 0.0;
    double                hist_vol_annualized = 0.0;
    std::vector<ScenarioResult> scenarios;
};

} // namespace macro
