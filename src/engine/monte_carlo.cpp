#include "engine/monte_carlo.h"

#include <cmath>
#include <random>

namespace macro::engine {

std::vector<double>
gbm_terminal_prices(double spot, double mu, double sigma,
                    int horizon_days, int paths, unsigned seed) {
    std::vector<double> out;
    out.reserve(static_cast<size_t>(paths));
    std::mt19937 rng(seed);
    std::normal_distribution<double> n(0.0, 1.0);
    for (int i = 0; i < paths; ++i) {
        double s = spot;
        for (int t = 0; t < horizon_days; ++t) {
            double z = n(rng);
            s *= std::exp((mu - 0.5 * sigma * sigma) + sigma * z);
        }
        out.push_back(s);
    }
    return out;
}

std::vector<double>
mixture_terminal_prices(double spot,
                        const std::vector<double>& event_returns,
                        const std::vector<double>& baseline_daily_returns,
                        int horizon_days,
                        int paths,
                        double weight_event,
                        unsigned seed) {
    std::vector<double> out;
    if (paths <= 0 || horizon_days <= 0) return out;
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> u(0.0, 1.0);
    out.reserve(static_cast<size_t>(paths));

    std::uniform_int_distribution<size_t> di_event(
        0, event_returns.empty() ? 0 : event_returns.size() - 1);
    std::uniform_int_distribution<size_t> di_base(
        0, baseline_daily_returns.empty() ? 0 : baseline_daily_returns.size() - 1);

    for (int i = 0; i < paths; ++i) {
        double s = spot;
        // Day 1 -> event window shock (drawn once)
        if (!event_returns.empty() && u(rng) < weight_event) {
            s *= std::exp(event_returns[di_event(rng)]);
        } else if (!baseline_daily_returns.empty()) {
            // Sum baseline daily log returns over `horizon_days`
            double acc = 0;
            for (int t = 0; t < horizon_days; ++t) {
                acc += baseline_daily_returns[di_base(rng)];
            }
            s *= std::exp(acc);
            out.push_back(s);
            continue;
        }
        // After the event shock, drift with baseline returns for remaining days
        if (!baseline_daily_returns.empty()) {
            for (int t = 1; t < horizon_days; ++t) {
                s *= std::exp(baseline_daily_returns[di_base(rng)]);
            }
        }
        out.push_back(s);
    }
    return out;
}

} // namespace macro::engine
