#include "engine/statistical_engine.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>

namespace macro::stats {

Summary summarize(const std::vector<double>& xs) {
    Summary s;
    s.n = xs.size();
    if (s.n == 0) return s;
    double sum = std::accumulate(xs.begin(), xs.end(), 0.0);
    s.mean = sum / static_cast<double>(s.n);
    double sq = 0.0;
    for (double v : xs) sq += (v - s.mean) * (v - s.mean);
    s.stddev = s.n > 1 ? std::sqrt(sq / static_cast<double>(s.n - 1)) : 0.0;
    auto sorted = xs;
    std::sort(sorted.begin(), sorted.end());
    s.min    = sorted.front();
    s.max    = sorted.back();
    s.median = quantile(sorted, 0.5);
    return s;
}

double quantile(std::vector<double> xs, double q) {
    if (xs.empty()) return 0.0;
    q = std::clamp(q, 0.0, 1.0);
    std::sort(xs.begin(), xs.end());
    double pos = q * static_cast<double>(xs.size() - 1);
    size_t lo = static_cast<size_t>(std::floor(pos));
    size_t hi = static_cast<size_t>(std::ceil(pos));
    double frac = pos - static_cast<double>(lo);
    return xs[lo] * (1.0 - frac) + xs[hi] * frac;
}

double phi(double z) {
    return 0.5 * std::erfc(-z * M_SQRT1_2);
}

double weighted_mean(const std::vector<double>& xs, const std::vector<double>& w) {
    if (xs.empty() || xs.size() != w.size()) return 0.0;
    double sw = 0.0, sxw = 0.0;
    for (std::size_t i = 0; i < xs.size(); ++i) { sw += w[i]; sxw += xs[i] * w[i]; }
    return sw > 0.0 ? sxw / sw : 0.0;
}

double weighted_quantile(std::vector<double> xs, std::vector<double> w, double q) {
    if (xs.empty() || xs.size() != w.size()) return 0.0;
    q = std::clamp(q, 0.0, 1.0);
    std::vector<std::size_t> idx(xs.size());
    for (std::size_t i = 0; i < idx.size(); ++i) idx[i] = i;
    std::sort(idx.begin(), idx.end(),
              [&](std::size_t a, std::size_t b) { return xs[a] < xs[b]; });
    double total = 0.0;
    for (double v : w) total += v;
    if (total <= 0.0) return 0.0;
    double cum = 0.0;
    for (std::size_t k = 0; k < idx.size(); ++k) {
        cum += w[idx[k]] / total;
        if (cum >= q) return xs[idx[k]];
    }
    return xs[idx.back()];
}

double weighted_prob_up(const std::vector<double>& xs, const std::vector<double>& w) {
    if (xs.empty() || xs.size() != w.size()) return 0.5;
    double up = 0.5, down = 0.5; // Jeffreys-style additive smoothing
    for (std::size_t i = 0; i < xs.size(); ++i) {
        if (xs[i] > 0) up   += w[i];
        else if (xs[i] < 0) down += w[i];
    }
    return up / (up + down);
}

std::vector<double> ewma_weights(std::size_t n, double half_life) {
    std::vector<double> w(n, 1.0);
    if (n == 0 || half_life <= 0.0) return w;
    double lambda = std::log(2.0) / half_life;
    for (std::size_t i = 0; i < n; ++i) {
        // i=0 oldest, i=n-1 newest -> weight grows with i
        double age = static_cast<double>(n - 1 - i);
        w[i] = std::exp(-lambda * age);
    }
    return w;
}

double sign_test_p(const std::vector<double>& xs) {
    if (xs.empty()) return 1.0;
    int n_pos = 0, n_neg = 0;
    for (double v : xs) {
        if (v > 0) ++n_pos;
        else if (v < 0) ++n_neg;
    }
    int n = n_pos + n_neg;
    if (n == 0) return 1.0;
    double p = 0.5;
    double mu = n * p;
    double sd = std::sqrt(n * p * (1 - p));
    int k = std::max(n_pos, n_neg);
    double z = (static_cast<double>(k) - 0.5 - mu) / sd; // continuity correction
    double tail = 1.0 - phi(z);
    return std::clamp(2.0 * tail, 0.0, 1.0);
}

CI bootstrap_mean_ci(const std::vector<double>& xs, double alpha, int iters, unsigned seed) {
    if (xs.empty()) return {0.0, 0.0};
    std::mt19937 rng(seed);
    std::uniform_int_distribution<size_t> dist(0, xs.size() - 1);
    std::vector<double> means;
    means.reserve(static_cast<size_t>(iters));
    for (int i = 0; i < iters; ++i) {
        double s = 0.0;
        for (size_t k = 0; k < xs.size(); ++k) s += xs[dist(rng)];
        means.push_back(s / static_cast<double>(xs.size()));
    }
    return {quantile(means, alpha / 2.0),
            quantile(means, 1.0 - alpha / 2.0)};
}

double posterior_prob_up(const std::vector<double>& xs) {
    // Jeffreys prior Beta(0.5, 0.5).
    double up = 0.5, down = 0.5;
    for (double v : xs) {
        if (v > 0) up += 1.0;
        else if (v < 0) down += 1.0;
    }
    return up / (up + down);
}

double historical_var(const std::vector<double>& xs, double alpha) {
    if (xs.empty()) return 0.0;
    double q = quantile(xs, alpha);
    return -q; // loss is positive
}

double expected_shortfall(const std::vector<double>& xs, double alpha) {
    if (xs.empty()) return 0.0;
    auto sorted = xs;
    std::sort(sorted.begin(), sorted.end());
    size_t cutoff = std::max<size_t>(1, static_cast<size_t>(alpha * sorted.size()));
    double s = 0.0;
    for (size_t i = 0; i < cutoff; ++i) s += sorted[i];
    return -s / static_cast<double>(cutoff);
}

double ols_slope(const std::vector<double>& x, const std::vector<double>& y) {
    if (x.size() != y.size() || x.size() < 2) return std::nan("");
    double mx = 0, my = 0;
    for (size_t i = 0; i < x.size(); ++i) { mx += x[i]; my += y[i]; }
    mx /= static_cast<double>(x.size());
    my /= static_cast<double>(x.size());
    double num = 0, den = 0;
    for (size_t i = 0; i < x.size(); ++i) {
        num += (x[i] - mx) * (y[i] - my);
        den += (x[i] - mx) * (x[i] - mx);
    }
    if (den == 0.0) return std::nan("");
    return num / den;
}

double t_test_p(const std::vector<double>& xs) {
    if (xs.size() < 2) return 1.0;
    auto s = summarize(xs);
    if (s.stddev == 0.0) return 1.0;
    double se = s.stddev / std::sqrt(static_cast<double>(s.n));
    double t  = s.mean / se;
    // For large n, t ~ N(0,1). Use normal approximation.
    return 2.0 * (1.0 - phi(std::abs(t)));
}

} // namespace macro::stats
