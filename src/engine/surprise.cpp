#include "engine/surprise.h"

#include <cmath>
#include <deque>

namespace macro::engine {

namespace {

double rolling_stdev(const std::deque<double>& w) {
    if (w.size() < 2) return std::nan("");
    double mean = 0.0;
    for (double v : w) mean += v;
    mean /= static_cast<double>(w.size());
    double s = 0.0;
    for (double v : w) s += (v - mean) * (v - mean);
    return std::sqrt(s / static_cast<double>(w.size() - 1));
}

} // namespace

std::vector<double>
standardised_surprise(const std::vector<HistoricalRelease>& releases, int window) {
    std::vector<double> out(releases.size(), std::nan(""));
    std::deque<double> w;
    for (std::size_t i = 0; i < releases.size(); ++i) {
        const auto& r = releases[i];
        if (std::isnan(r.consensus)) continue;
        double raw = r.actual - r.consensus;
        double sd  = rolling_stdev(w);
        if (!std::isnan(sd) && sd > 0.0) {
            out[i] = raw / sd;
        }
        w.push_back(raw);
        if (static_cast<int>(w.size()) > window) w.pop_front();
    }
    return out;
}

double standardise_one(const std::vector<HistoricalRelease>& history,
                       time::Date as_of,
                       double raw_surprise,
                       int window) {
    std::deque<double> w;
    for (const auto& r : history) {
        if (r.date >= as_of) break;
        if (std::isnan(r.consensus)) continue;
        w.push_back(r.actual - r.consensus);
        if (static_cast<int>(w.size()) > window) w.pop_front();
    }
    double sd = rolling_stdev(w);
    if (std::isnan(sd) || sd <= 0.0) return std::nan("");
    return raw_surprise / sd;
}

} // namespace macro::engine
