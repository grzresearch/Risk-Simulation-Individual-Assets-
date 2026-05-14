#pragma once

#include <vector>

namespace macro::engine {

// GARCH(1,1):  sigma_t^2 = omega + alpha * eps_{t-1}^2 + beta * sigma_{t-1}^2
struct GarchParams {
    double omega = 0.0;
    double alpha = 0.0;
    double beta  = 0.0;
    double log_likelihood = 0.0;
    bool   converged = false;
    int    iters = 0;
};

// Estimate via Nelder-Mead on the log-likelihood with sensible bounds.
// `returns` should be demeaned log returns. Falls back to RiskMetrics
// lambda=0.94 when MLE fails.
GarchParams fit_garch11(const std::vector<double>& returns);

// One-step-ahead conditional variance forecast for the next observation,
// given fitted params and the realised return path. Returns sigma (not sigma^2).
double forecast_sigma_one_step(const GarchParams& p, const std::vector<double>& returns);

// Annualise daily sigma (sqrt(252)).
double annualise(double daily_sigma);

} // namespace macro::engine
