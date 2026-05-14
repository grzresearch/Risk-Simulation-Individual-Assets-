#include "engine/garch.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>

namespace macro::engine {

namespace {

// Negative log-likelihood of GARCH(1,1) under conditional Gaussian shocks.
// Penalises invalid parameter regions so the simplex stays in the feasible
// set (omega>0, alpha>=0, beta>=0, alpha+beta<1).
double neg_log_lik(const std::array<double, 3>& p, const std::vector<double>& r) {
    double omega = p[0], alpha = p[1], beta = p[2];
    if (omega <= 1e-12 || alpha < 0 || beta < 0 || alpha + beta >= 0.999) {
        return 1e10;
    }
    double sample_var = 0.0;
    for (double v : r) sample_var += v * v;
    sample_var = r.empty() ? 1e-6 : sample_var / static_cast<double>(r.size());
    double s2 = sample_var;
    double nll = 0.0;
    for (double e : r) {
        s2 = omega + alpha * e * e + beta * s2;
        if (s2 <= 0.0) return 1e10;
        nll += 0.5 * (std::log(2.0 * M_PI) + std::log(s2) + (e * e) / s2);
    }
    return nll;
}

// Minimal Nelder-Mead in 3 dimensions.
GarchParams nelder_mead(const std::vector<double>& r) {
    GarchParams out;
    const int N = 3;
    using Vec = std::array<double, 3>;
    // Initial simplex (sample-based starting values from RiskMetrics-ish guess)
    Vec x0 = {1e-6, 0.06, 0.92};
    std::array<Vec, 4> simplex = {x0, x0, x0, x0};
    simplex[1][0] *= 2.0;
    simplex[2][1] += 0.05;
    simplex[3][2] -= 0.05;

    std::array<double, 4> f{};
    for (int i = 0; i <= N; ++i) f[i] = neg_log_lik(simplex[i], r);

    const int max_iters = 400;
    const double tol = 1e-7;
    auto sort_idx = [&](std::array<int, 4>& idx) {
        for (int i = 0; i <= N; ++i) idx[i] = i;
        std::sort(idx.begin(), idx.begin() + N + 1,
                  [&](int a, int b) { return f[a] < f[b]; });
    };

    int it = 0;
    for (; it < max_iters; ++it) {
        std::array<int, 4> idx{};
        sort_idx(idx);
        if (std::fabs(f[idx[N]] - f[idx[0]]) < tol) break;

        // centroid of best N
        Vec centroid = {0, 0, 0};
        for (int i = 0; i < N; ++i)
            for (int d = 0; d < N; ++d)
                centroid[d] += simplex[idx[i]][d] / static_cast<double>(N);

        Vec worst = simplex[idx[N]];
        Vec xr; for (int d = 0; d < N; ++d) xr[d] = centroid[d] + (centroid[d] - worst[d]);
        double fr = neg_log_lik(xr, r);

        if (f[idx[0]] <= fr && fr < f[idx[N - 1]]) {
            simplex[idx[N]] = xr; f[idx[N]] = fr;
        } else if (fr < f[idx[0]]) {
            Vec xe; for (int d = 0; d < N; ++d) xe[d] = centroid[d] + 2.0 * (xr[d] - centroid[d]);
            double fe = neg_log_lik(xe, r);
            if (fe < fr) { simplex[idx[N]] = xe; f[idx[N]] = fe; }
            else         { simplex[idx[N]] = xr; f[idx[N]] = fr; }
        } else {
            Vec xc; for (int d = 0; d < N; ++d) xc[d] = centroid[d] + 0.5 * (worst[d] - centroid[d]);
            double fc = neg_log_lik(xc, r);
            if (fc < f[idx[N]]) {
                simplex[idx[N]] = xc; f[idx[N]] = fc;
            } else {
                // shrink toward best
                Vec best = simplex[idx[0]];
                for (int i = 1; i <= N; ++i) {
                    for (int d = 0; d < N; ++d)
                        simplex[idx[i]][d] = best[d] + 0.5 * (simplex[idx[i]][d] - best[d]);
                    f[idx[i]] = neg_log_lik(simplex[idx[i]], r);
                }
            }
        }
    }
    std::array<int, 4> idx{};
    sort_idx(idx);
    Vec best = simplex[idx[0]];
    out.omega = best[0]; out.alpha = best[1]; out.beta = best[2];
    out.log_likelihood = -f[idx[0]];
    out.converged = (it < max_iters);
    out.iters = it;
    return out;
}

} // namespace

GarchParams fit_garch11(const std::vector<double>& returns) {
    GarchParams params;
    if (returns.size() < 50) {
        // Fallback to RiskMetrics with lambda = 0.94
        params.omega = 0.0;
        params.alpha = 0.06;
        params.beta  = 0.94;
        return params;
    }
    double mean = std::accumulate(returns.begin(), returns.end(), 0.0) /
                  static_cast<double>(returns.size());
    std::vector<double> centred(returns.size());
    for (std::size_t i = 0; i < returns.size(); ++i) centred[i] = returns[i] - mean;
    auto fit = nelder_mead(centred);
    if (!fit.converged || fit.alpha + fit.beta >= 0.999) {
        // Fallback
        params.omega = 0.0;
        params.alpha = 0.06;
        params.beta  = 0.94;
        return params;
    }
    return fit;
}

double forecast_sigma_one_step(const GarchParams& p, const std::vector<double>& returns) {
    if (returns.empty()) return 0.0;
    double s2 = 0.0;
    for (double v : returns) s2 += v * v;
    s2 /= static_cast<double>(returns.size());
    for (double e : returns) {
        s2 = p.omega + p.alpha * e * e + p.beta * s2;
        if (s2 <= 0.0) return 0.0;
    }
    return std::sqrt(s2);
}

double annualise(double daily_sigma) {
    return daily_sigma * std::sqrt(252.0);
}

} // namespace macro::engine
