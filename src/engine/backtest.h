#pragma once

#include "engine/calibration.h"
#include "models.h"

#include <string>
#include <vector>

namespace macro::engine {

struct BacktestResult {
    std::string event_kind;
    int    horizon_days = 5;
    int    n_train_min = 0;
    int    n_events = 0;
    CalibrationStats calib;
    double pnl_total = 0.0;          // sum of log returns when forecast P(up) > 0.5 (long), * sign
    double pnl_sharpe = 0.0;         // annualised
    double pnl_sortino = 0.0;
    double max_drawdown = 0.0;
    double hit_rate_signed = 0.0;    // includes shorts (P<0.5)
};

// Walk-forward back-test: for each historical event i in chronological order
// after the warm-up `min_train_size`, build a forecast using only events
// strictly before i, then evaluate against the realised return at i.
//
// `prices` is the asset's daily history. `releases` is the list of
// historical macro releases of the kind under test.
BacktestResult walk_forward(const std::string& event_kind,
                            const PriceSeries& prices,
                            const std::vector<HistoricalRelease>& releases,
                            int horizon_days = 5,
                            int min_train_size = 24);

} // namespace macro::engine
