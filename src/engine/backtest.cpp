#include "engine/backtest.h"

#include "engine/historical_analyzer.h"
#include "engine/risk_calculator.h"
#include "engine/statistical_engine.h"

#include <algorithm>
#include <cmath>

namespace macro::engine {

namespace {

double sharpe_from_returns(const std::vector<double>& r, double risk_free = 0.0) {
    if (r.size() < 2) return 0.0;
    double mean = 0.0;
    for (double v : r) mean += v;
    mean /= static_cast<double>(r.size());
    double sq = 0.0;
    for (double v : r) sq += (v - mean) * (v - mean);
    double sd = std::sqrt(sq / static_cast<double>(r.size() - 1));
    if (sd == 0.0) return 0.0;
    // Approx 52 events/year for monthly cadence; rescale generously.
    return (mean - risk_free) / sd * std::sqrt(12.0);
}

double sortino_from_returns(const std::vector<double>& r) {
    if (r.size() < 2) return 0.0;
    double mean = 0.0;
    for (double v : r) mean += v;
    mean /= static_cast<double>(r.size());
    double sq = 0.0;
    int n_neg = 0;
    for (double v : r) {
        if (v < 0) { sq += v * v; ++n_neg; }
    }
    if (n_neg < 2) return 0.0;
    double dd = std::sqrt(sq / static_cast<double>(n_neg));
    return dd == 0.0 ? 0.0 : mean / dd * std::sqrt(12.0);
}

double max_drawdown(const std::vector<double>& cum_pnl) {
    double peak = 0.0, mdd = 0.0;
    for (double v : cum_pnl) {
        peak = std::max(peak, v);
        mdd  = std::min(mdd, v - peak);
    }
    return mdd;
}

} // namespace

BacktestResult walk_forward(const std::string& event_kind,
                            const PriceSeries& prices,
                            const std::vector<HistoricalRelease>& releases,
                            int horizon_days,
                            int min_train_size) {
    BacktestResult res;
    res.event_kind = event_kind;
    res.horizon_days = horizon_days;
    res.n_train_min = min_train_size;
    if (releases.size() < static_cast<std::size_t>(min_train_size + 5) || prices.size() < 60) {
        return res;
    }

    std::vector<double> probs;
    std::vector<int>    outcomes;
    std::vector<double> strategy_returns;

    for (std::size_t i = static_cast<std::size_t>(min_train_size); i < releases.size(); ++i) {
        // Train on releases[0..i-1]
        std::vector<HistoricalRelease> train(releases.begin(), releases.begin() + i);
        auto train_reactions = event_window_returns(prices, train, {1, 5, 20});
        auto train_sample    = returns_at_horizon(train_reactions, horizon_days);
        if (train_sample.size() < 6) continue;

        double p_up = stats::posterior_prob_up(train_sample);
        // Test on releases[i]
        long base = index_at_or_after(prices, releases[i].date);
        if (base < 1) continue;
        long post = base + horizon_days;
        if (post >= static_cast<long>(prices.size())) continue;
        double p0 = prices[base - 1].close;
        double p1 = prices[post].close;
        if (p0 <= 0 || p1 <= 0) continue;
        double realised = std::log(p1 / p0);
        int outcome = (realised > 0) ? 1 : 0;

        probs.push_back(p_up);
        outcomes.push_back(outcome);

        // Strategy: long if P(up) > 0.5, short otherwise. Size = |p - 0.5| * 2 (in [0,1]).
        double size = std::fabs(p_up - 0.5) * 2.0;
        double sign = (p_up > 0.5) ? 1.0 : -1.0;
        strategy_returns.push_back(sign * size * realised);
    }

    res.n_events = static_cast<int>(probs.size());
    if (res.n_events == 0) return res;
    res.calib = score(probs, outcomes);

    int hits = 0;
    for (std::size_t k = 0; k < probs.size(); ++k) {
        bool predicted_up = probs[k] > 0.5;
        if ((predicted_up && outcomes[k] == 1) || (!predicted_up && outcomes[k] == 0)) ++hits;
    }
    res.hit_rate_signed = static_cast<double>(hits) / static_cast<double>(probs.size());

    double total = 0.0;
    std::vector<double> cum;
    cum.reserve(strategy_returns.size());
    for (double v : strategy_returns) { total += v; cum.push_back(total); }
    res.pnl_total    = total;
    res.pnl_sharpe   = sharpe_from_returns(strategy_returns);
    res.pnl_sortino  = sortino_from_returns(strategy_returns);
    res.max_drawdown = max_drawdown(cum);
    return res;
}

} // namespace macro::engine
