#pragma once

#include "models.h"

#include <optional>
#include <vector>

namespace macro::engine {

// Computes log returns N business days after each historical event date.
// Filters out events outside the price coverage window.
std::vector<EventReaction>
event_window_returns(const PriceSeries& prices,
                     const std::vector<HistoricalRelease>& releases,
                     const std::vector<int>& horizons);

// Log returns from prices.
std::vector<double> log_returns(const PriceSeries& prices);

// Annualized standard deviation of daily log returns (sqrt(252)).
double annualized_vol(const PriceSeries& prices);

// Find the price index at or just after a given date. Returns -1 if not found.
long index_at_or_after(const PriceSeries& prices, time::Date d);

} // namespace macro::engine
