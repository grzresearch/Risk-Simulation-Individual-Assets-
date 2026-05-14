#pragma once

#include <vector>

namespace macro::engine {

struct CalibrationStats {
    int    n = 0;
    double brier = 0.0;     // sum (p - y)^2 / n
    double log_loss = 0.0;  // mean negative log-likelihood
    double hit_rate = 0.0;  // share of correct directional calls (p>=0.5)
    double avg_prob_up = 0.0;
    double base_rate = 0.0; // empirical share of ups
};

// `probs` are forecast P(up), `outcomes` are 1 if return>0 else 0.
CalibrationStats score(const std::vector<double>& probs,
                       const std::vector<int>& outcomes);

struct ReliabilityBin {
    double prob_lo = 0.0;
    double prob_hi = 0.0;
    double mean_pred = 0.0;
    double empirical = 0.0;
    int    n = 0;
};

std::vector<ReliabilityBin>
reliability_bins(const std::vector<double>& probs,
                 const std::vector<int>& outcomes,
                 int bins = 10);

} // namespace macro::engine
