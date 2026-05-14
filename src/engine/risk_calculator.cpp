#include "engine/risk_calculator.h"

#include "engine/statistical_engine.h"

#include <cmath>

namespace macro::engine {

std::vector<double>
returns_at_horizon(const std::vector<EventReaction>& reactions, int horizon) {
    std::vector<double> out;
    out.reserve(reactions.size());
    auto pick = [&](const EventReaction& er) {
        if (horizon <= 1)      return er.return_1d;
        else if (horizon <= 5) return er.return_5d;
        else                   return er.return_20d;
    };
    for (const auto& r : reactions) {
        double v = pick(r);
        if (v != 0.0) out.push_back(v);
    }
    return out;
}

DirectionalForecast forecast_from_sample(const std::vector<double>& sample) {
    DirectionalForecast f;
    f.sample_size = static_cast<int>(sample.size());
    if (sample.empty()) return f;
    auto s = stats::summarize(sample);
    f.mean_return = s.mean;
    f.median_return = s.median;
    f.stddev = s.stddev;
    f.prob_up = stats::posterior_prob_up(sample);
    f.prob_down = 1.0 - f.prob_up;
    f.ci68_low  = stats::quantile(sample, 0.16);
    f.ci68_high = stats::quantile(sample, 0.84);
    f.ci95_low  = stats::quantile(sample, 0.025);
    f.ci95_high = stats::quantile(sample, 0.975);
    f.var95 = stats::historical_var(sample, 0.05);
    f.es95  = stats::expected_shortfall(sample, 0.05);
    f.sign_test_p = stats::sign_test_p(sample);
    return f;
}

std::optional<double>
surprise_sensitivity(const std::vector<EventReaction>& reactions, int horizon) {
    std::vector<double> x, y;
    auto pick = [&](const EventReaction& er) {
        if (horizon <= 1)      return er.return_1d;
        else if (horizon <= 5) return er.return_5d;
        else                   return er.return_20d;
    };
    for (const auto& r : reactions) {
        if (r.surprise == 0.0) continue;
        double yv = pick(r);
        if (yv == 0.0) continue;
        x.push_back(r.surprise);
        y.push_back(yv);
    }
    if (x.size() < 5) return std::nullopt;
    double slope = stats::ols_slope(x, y);
    if (std::isnan(slope)) return std::nullopt;
    return slope;
}

} // namespace macro::engine
