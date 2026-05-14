#pragma once

#include "models.h"
#include "util/time_utils.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace macro::engine {

enum class Regime {
    Easing,        // FFR falling or low, CPI < 3
    Neutral,
    Hiking,        // FFR rising and CPI > 3
    Stagflation,   // CPI > 5 and PMI < 50 / Unemployment > 5
    Risk_Off,      // VIX > 25 or sharp equity drawdown
    Unknown,
};

const char* to_string(Regime r);

struct RegimeContext {
    // Macro signal values used for classification (latest known).
    std::optional<double> cpi_yoy;        // %
    std::optional<double> fed_funds;      // %
    std::optional<double> unemployment;   // %
    std::optional<double> vix;            // index
    std::optional<double> equity_dd_3m;   // SPY drawdown over 3 months (e.g. -0.10)
};

// Classify the regime as of a given date using macro time series.
Regime classify_regime(const RegimeContext& ctx);

// Build a chronological vector of (date, regime) labels for back-dated
// classification. macro series passed in date->value maps.
struct RegimeHistory {
    std::vector<time::Date> dates;
    std::vector<Regime>     regimes;
};

RegimeHistory build_regime_history(
    const std::map<std::string /*yyyy-mm*/, double>& cpi_yoy,
    const std::map<std::string, double>& fed_funds,
    const std::map<std::string, double>& unemployment,
    const std::map<std::string, double>& vix,
    const std::map<std::string, double>& spy_dd_3m,
    time::Date from, time::Date to);

// Return the regime active at `d`, or Unknown if before history.
Regime regime_at(const RegimeHistory& h, time::Date d);

// Filter a sample of (event_date, value) to those that occurred while the
// regime matched `target`. Index-paired with `dates`.
std::vector<double>
filter_by_regime(const std::vector<time::Date>& dates,
                 const std::vector<double>& values,
                 const RegimeHistory& h,
                 Regime target);

} // namespace macro::engine
