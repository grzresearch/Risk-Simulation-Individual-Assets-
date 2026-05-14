#pragma once

#include <cstddef>
#include <vector>

namespace macro::stats {

struct Summary {
    double mean = 0.0;
    double median = 0.0;
    double stddev = 0.0;
    double min = 0.0;
    double max = 0.0;
    std::size_t n = 0;
};

Summary summarize(const std::vector<double>& xs);

// Quantile via linear interpolation. q in [0,1].
double quantile(std::vector<double> xs, double q);

// Standard normal CDF.
double phi(double z);

// Two-sided sign test p-value: H0 = median of xs is 0.
double sign_test_p(const std::vector<double>& xs);

// Bootstrap CI for the mean. Returns {low, high}.
struct CI { double low; double high; };
CI bootstrap_mean_ci(const std::vector<double>& xs, double alpha, int iters = 2000, unsigned seed = 42);

// Probability that the next return is > 0 using a Bayesian Beta-Binomial
// posterior (Jeffreys prior). Returns P(up | sample).
double posterior_prob_up(const std::vector<double>& xs);

// Historical VaR / Expected Shortfall at given alpha (e.g. 0.05).
double historical_var(const std::vector<double>& xs, double alpha);
double expected_shortfall(const std::vector<double>& xs, double alpha);

// Simple OLS slope of y on x. Returns NaN if undefined.
double ols_slope(const std::vector<double>& x, const std::vector<double>& y);

// Approximate one-sample t-test p-value (two-sided) for H0: mean=0.
double t_test_p(const std::vector<double>& xs);

} // namespace macro::stats
