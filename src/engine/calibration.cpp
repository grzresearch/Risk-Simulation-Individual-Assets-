#include "engine/calibration.h"

#include <algorithm>
#include <cmath>

namespace macro::engine {

CalibrationStats score(const std::vector<double>& probs,
                       const std::vector<int>& outcomes) {
    CalibrationStats s;
    s.n = static_cast<int>(std::min(probs.size(), outcomes.size()));
    if (s.n == 0) return s;
    double bsum = 0.0, lsum = 0.0, hits = 0.0, psum = 0.0, ysum = 0.0;
    for (int i = 0; i < s.n; ++i) {
        double p = std::clamp(probs[i], 1e-6, 1.0 - 1e-6);
        int    y = outcomes[i] ? 1 : 0;
        bsum += (p - y) * (p - y);
        lsum += -(y * std::log(p) + (1 - y) * std::log(1.0 - p));
        psum += p;
        ysum += y;
        if ((p >= 0.5 && y == 1) || (p < 0.5 && y == 0)) hits += 1.0;
    }
    s.brier        = bsum / s.n;
    s.log_loss     = lsum / s.n;
    s.hit_rate     = hits / s.n;
    s.avg_prob_up  = psum / s.n;
    s.base_rate    = ysum / s.n;
    return s;
}

std::vector<ReliabilityBin>
reliability_bins(const std::vector<double>& probs,
                 const std::vector<int>& outcomes,
                 int bins) {
    std::vector<ReliabilityBin> out(static_cast<std::size_t>(bins));
    const double w = 1.0 / bins;
    for (int b = 0; b < bins; ++b) {
        out[b].prob_lo = b * w;
        out[b].prob_hi = (b + 1) * w;
    }
    const std::size_t n = std::min(probs.size(), outcomes.size());
    for (std::size_t i = 0; i < n; ++i) {
        int b = std::min(bins - 1, static_cast<int>(probs[i] * bins));
        b = std::max(0, b);
        out[b].mean_pred += probs[i];
        out[b].empirical += outcomes[i];
        out[b].n += 1;
    }
    for (auto& bin : out) {
        if (bin.n > 0) {
            bin.mean_pred /= bin.n;
            bin.empirical /= bin.n;
        }
    }
    return out;
}

} // namespace macro::engine
