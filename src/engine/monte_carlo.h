#pragma once

#include <vector>

namespace macro::engine {

// Single-asset geometric Brownian motion Monte-Carlo. Returns the simulated
// terminal price distribution.
//
// spot:    starting price
// mu:      drift per day (log return)
// sigma:   stdev per day (log return)
// horizon: number of days
// paths:   number of paths
std::vector<double>
gbm_terminal_prices(double spot, double mu, double sigma,
                    int horizon_days, int paths, unsigned seed = 1234);

// Mix two distributions: with probability `weight_event` draw from the
// event-conditional return sample (with replacement); with the complementary
// weight, draw from the unconditional empirical return distribution.
std::vector<double>
mixture_terminal_prices(double spot,
                        const std::vector<double>& event_returns,
                        const std::vector<double>& baseline_daily_returns,
                        int horizon_days,
                        int paths,
                        double weight_event = 0.5,
                        unsigned seed = 1234);

} // namespace macro::engine
