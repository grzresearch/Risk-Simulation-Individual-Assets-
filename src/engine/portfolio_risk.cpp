#include "engine/portfolio_risk.h"

#include "engine/statistical_engine.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <unordered_map>

namespace macro::engine {

namespace {

std::vector<double> simulate_portfolio_pnl(
    const std::vector<double>& weights,
    const linalg::Matrix& L,
    const std::vector<double>& mu_per_day,
    int horizon_days,
    int paths,
    unsigned seed) {
    const std::size_t k = weights.size();
    std::vector<double> out;
    out.reserve(static_cast<std::size_t>(paths));
    std::mt19937 rng(seed);
    std::normal_distribution<double> normal(0.0, 1.0);

    for (int p = 0; p < paths; ++p) {
        std::vector<double> cum_log_ret(k, 0.0);
        for (int t = 0; t < horizon_days; ++t) {
            std::vector<double> z(k);
            for (std::size_t i = 0; i < k; ++i) z[i] = normal(rng);
            auto shock = linalg::lt_mul(L, z);
            for (std::size_t i = 0; i < k; ++i) {
                cum_log_ret[i] += mu_per_day[i] + shock[i];
            }
        }
        double pnl = 0.0;
        for (std::size_t i = 0; i < k; ++i) {
            // Linearise via simple return for portfolio-level PnL
            pnl += weights[i] * (std::exp(cum_log_ret[i]) - 1.0);
        }
        out.push_back(pnl);
    }
    return out;
}

} // namespace

PortfolioRiskReport
portfolio_risk(const std::vector<PortfolioHolding>& holdings,
               const std::vector<std::string>& asset_ids,
               const std::vector<std::vector<double>>& aligned_returns,
               int horizon_days,
               int mc_paths,
               unsigned seed) {
    PortfolioRiskReport rep;
    rep.horizon_days = static_cast<double>(horizon_days);

    std::unordered_map<std::string, std::size_t> idx_of;
    for (std::size_t i = 0; i < asset_ids.size(); ++i) idx_of[asset_ids[i]] = i;

    std::vector<double> weights(asset_ids.size(), 0.0);
    for (const auto& h : holdings) {
        auto it = idx_of.find(h.asset_id);
        if (it == idx_of.end()) continue;
        weights[it->second] = h.weight;
    }
    rep.asset_ids = asset_ids;
    rep.weights   = weights;
    rep.n_assets  = static_cast<int>(asset_ids.size());

    if (aligned_returns.empty() || asset_ids.empty()) return rep;

    auto cov = linalg::covariance(aligned_returns);
    rep.correlation = linalg::correlation(aligned_returns);
    rep.correl_labels = asset_ids;
    auto L   = linalg::cholesky(cov);

    std::vector<double> mu(asset_ids.size(), 0.0);
    for (std::size_t i = 0; i < asset_ids.size(); ++i) {
        if (aligned_returns[i].empty()) continue;
        double s = 0.0;
        for (double v : aligned_returns[i]) s += v;
        mu[i] = s / static_cast<double>(aligned_returns[i].size());
    }

    auto pnl = simulate_portfolio_pnl(weights, L, mu, horizon_days, mc_paths, seed);
    auto sx  = stats::summarize(pnl);
    rep.mean_return = sx.mean;
    rep.stddev      = sx.stddev;
    rep.var95 = stats::historical_var(pnl, 0.05);
    rep.var99 = stats::historical_var(pnl, 0.01);
    rep.es95  = stats::expected_shortfall(pnl, 0.05);
    rep.es99  = stats::expected_shortfall(pnl, 0.01);

    // Component VaR via finite-difference marginal contributions.
    rep.component_var95.assign(asset_ids.size(), 0.0);
    rep.marginal_var95.assign(asset_ids.size(), 0.0);
    const double bump = 0.01; // 1% weight shift
    for (std::size_t i = 0; i < asset_ids.size(); ++i) {
        auto w_up = weights;
        w_up[i] += bump;
        auto pnl_up = simulate_portfolio_pnl(w_up, L, mu, horizon_days, mc_paths / 2,
                                             seed + static_cast<unsigned>(i) + 1u);
        double var_up = stats::historical_var(pnl_up, 0.05);
        double marginal = (var_up - rep.var95) / bump;
        rep.marginal_var95[i]  = marginal;
        rep.component_var95[i] = marginal * weights[i];
    }
    return rep;
}

} // namespace macro::engine
