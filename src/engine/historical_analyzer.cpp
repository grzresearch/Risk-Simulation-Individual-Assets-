#include "engine/historical_analyzer.h"

#include <algorithm>
#include <cmath>

namespace macro::engine {

std::vector<double> log_returns(const PriceSeries& p) {
    std::vector<double> out;
    if (p.size() < 2) return out;
    out.reserve(p.size() - 1);
    for (size_t i = 1; i < p.size(); ++i) {
        if (p[i - 1].close <= 0.0 || p[i].close <= 0.0) continue;
        out.push_back(std::log(p[i].close / p[i - 1].close));
    }
    return out;
}

double annualized_vol(const PriceSeries& p) {
    auto r = log_returns(p);
    if (r.size() < 2) return 0.0;
    double mean = 0.0;
    for (double v : r) mean += v;
    mean /= static_cast<double>(r.size());
    double sq = 0.0;
    for (double v : r) sq += (v - mean) * (v - mean);
    double sd = std::sqrt(sq / static_cast<double>(r.size() - 1));
    return sd * std::sqrt(252.0);
}

long index_at_or_after(const PriceSeries& prices, time::Date d) {
    auto it = std::lower_bound(prices.begin(), prices.end(), d,
        [](const PricePoint& p, time::Date dd) { return p.date < dd; });
    if (it == prices.end()) return -1;
    return static_cast<long>(it - prices.begin());
}

std::vector<EventReaction>
event_window_returns(const PriceSeries& prices,
                     const std::vector<HistoricalRelease>& releases,
                     const std::vector<int>& horizons) {
    std::vector<EventReaction> out;
    if (prices.size() < 25 || releases.empty()) return out;

    auto at_horizon = [&](long base, int h) -> double {
        long idx_pre  = base - 1;
        long idx_post = base + h;
        if (idx_pre < 0 || idx_post >= static_cast<long>(prices.size())) {
            return std::nan("");
        }
        double p0 = prices[idx_pre].close;
        double p1 = prices[idx_post].close;
        if (p0 <= 0 || p1 <= 0) return std::nan("");
        return std::log(p1 / p0);
    };

    for (const auto& r : releases) {
        long base = index_at_or_after(prices, r.date);
        if (base < 0) continue;
        EventReaction er;
        er.event_date = r.date;
        er.surprise   = std::isnan(r.consensus) ? 0.0 : (r.actual - r.consensus);
        // Apply requested horizons; only 1, 5, 20 are stored in the struct,
        // but caller can choose horizons.
        for (int h : horizons) {
            double ret = at_horizon(base, h);
            if (std::isnan(ret)) continue;
            if (h == 1)       er.return_1d  = ret;
            else if (h == 5)  er.return_5d  = ret;
            else if (h == 20) er.return_20d = ret;
        }
        out.push_back(er);
    }
    return out;
}

} // namespace macro::engine
