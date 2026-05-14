#include "engine/statistical_engine.h"
#include "engine/monte_carlo.h"
#include "engine/risk_calculator.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

static void approx(double a, double b, double tol, const char* msg) {
    if (std::fabs(a - b) > tol) {
        std::cerr << "FAIL " << msg << ": " << a << " vs " << b << "\n";
        std::exit(1);
    }
}

int main() {
    using namespace macro;
    {
        std::vector<double> xs = {-0.02, -0.01, 0.0, 0.01, 0.02, 0.03};
        auto s = stats::summarize(xs);
        approx(s.mean, 0.005, 1e-9, "mean");
        approx(s.n, 6, 0, "n");
    }
    {
        std::vector<double> xs = {-3, -2, -1, 1, 2, 3};
        approx(stats::quantile(xs, 0.5), 0.0, 1e-9, "median");
        approx(stats::quantile(xs, 0.0), -3.0, 1e-9, "q0");
        approx(stats::quantile(xs, 1.0),  3.0, 1e-9, "q1");
    }
    {
        // 45 up vs 5 down should trip the sign test convincingly.
        std::vector<double> xs(50, 0.01);
        for (size_t i = 0; i < 5; ++i) xs[i] = -0.005;
        auto f = engine::forecast_from_sample(xs);
        if (f.prob_up <= 0.7) { std::cerr << "expected prob_up>0.7, got " << f.prob_up << "\n"; std::exit(1); }
        if (f.sign_test_p >= 0.01) { std::cerr << "expected sign_test_p<0.01, got " << f.sign_test_p << "\n"; std::exit(1); }
    }
    {
        auto terms = engine::gbm_terminal_prices(100, 0.0, 0.01, 5, 5000, 7);
        assert(!terms.empty());
        double p50 = stats::quantile(terms, 0.5);
        approx(p50, 100.0, 5.0, "MC median around spot");
    }
    {
        std::vector<double> x = {-1, 0, 1, 2, 3};
        std::vector<double> y = {-2, 0, 2, 4, 6};
        approx(stats::ols_slope(x, y), 2.0, 1e-9, "slope=2");
    }
    {
        // 10 samples -> 10th percentile with linear interpolation sits between
        // the two lowest observations (-10% and -5%).
        std::vector<double> xs = {-0.10, -0.05, -0.02, 0.0, 0.01, 0.02, 0.03, 0.05, 0.07, 0.1};
        approx(stats::historical_var(xs, 0.1), 0.055, 1e-9, "VaR10");
    }
    std::cout << "OK\n";
    return 0;
}
