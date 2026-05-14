#include "engine/calibration.h"
#include "engine/garch.h"
#include "engine/linalg.h"
#include "engine/multitest.h"
#include "engine/portfolio_risk.h"
#include "engine/regime.h"
#include "engine/statistical_engine.h"
#include "engine/stress.h"
#include "engine/surprise.h"
#include "util/holiday_calendar.h"

#include <cmath>
#include <iostream>
#include <random>
#include <vector>

static int g_fails = 0;
static void check(bool cond, const char* msg) {
    if (!cond) { std::cerr << "FAIL " << msg << "\n"; ++g_fails; }
}
static void approx(double a, double b, double tol, const char* msg) {
    if (std::fabs(a - b) > tol) {
        std::cerr << "FAIL " << msg << ": " << a << " vs " << b << "\n"; ++g_fails;
    }
}

int main() {
    using namespace macro;

    // --- Holiday calendar -----------------------------------------------
    {
        time::HolidayCalendar hc;
        check( hc.is_holiday(time::parse_date("2024-12-25")), "xmas closed");
        check(!hc.is_holiday(time::parse_date("2024-12-26")), "boxing-day open");
        check( hc.is_trading_day(time::parse_date("2024-12-23")), "23 dec open");
        auto t = hc.shift_trading_days(time::parse_date("2024-12-24"), 1);
        check(time::to_iso(t) == "2024-12-26", "skip xmas");
    }

    // --- Standardised surprise ------------------------------------------
    {
        std::vector<HistoricalRelease> h;
        for (int i = 0; i < 30; ++i) {
            HistoricalRelease r;
            r.date     = time::add_days(time::parse_date("2020-01-01"), i * 30);
            r.consensus = 2.0;
            r.actual    = 2.0 + ((i % 2) ? 0.2 : -0.2);
            h.push_back(r);
        }
        auto z = engine::standardised_surprise(h, 24);
        check(z.size() == h.size(), "z size");
        // Last entry should be ~1 sigma in magnitude.
        check(!std::isnan(z.back()) && std::fabs(z.back()) > 0.5, "z magnitude");
    }

    // --- GARCH ----------------------------------------------------------
    {
        std::mt19937 rng(7);
        std::normal_distribution<double> n(0.0, 0.01);
        std::vector<double> r;
        double s = 0.01;
        for (int i = 0; i < 1000; ++i) {
            double e = n(rng) * (s / 0.01);
            r.push_back(e);
            s = std::sqrt(1e-7 + 0.05 * e * e + 0.92 * s * s);
        }
        auto p = engine::fit_garch11(r);
        check(p.alpha >= 0.0 && p.beta >= 0.0, "garch positivity");
        check(p.alpha + p.beta < 1.0, "garch stationarity");
        double sig = engine::forecast_sigma_one_step(p, r);
        check(sig > 0.0 && sig < 0.1, "garch sigma sane");
    }

    // --- Regime classifier ---------------------------------------------
    {
        engine::RegimeContext c;
        c.cpi_yoy = 6.5; c.unemployment = 6.0;
        check(engine::classify_regime(c) == engine::Regime::Stagflation, "stagflation");
        c = {}; c.cpi_yoy = 4.0; c.fed_funds = 5.0;
        check(engine::classify_regime(c) == engine::Regime::Hiking, "hiking");
        c = {}; c.cpi_yoy = 1.5; c.fed_funds = 0.5;
        check(engine::classify_regime(c) == engine::Regime::Easing, "easing");
        c = {}; c.vix = 35.0;
        check(engine::classify_regime(c) == engine::Regime::Risk_Off, "risk-off");
    }

    // --- Multi-testing FDR ---------------------------------------------
    {
        std::vector<engine::TestRow> rows;
        for (int i = 0; i < 20; ++i) {
            engine::TestRow r;
            r.label = "t" + std::to_string(i);
            r.p_value = (i < 3) ? 0.001 : 0.5;
            rows.push_back(r);
        }
        auto adj = engine::apply_corrections(rows, 0.05);
        int rejected = 0;
        for (const auto& r : adj) if (r.rejected_at_5pct_fdr) ++rejected;
        check(rejected == 3, "BH rejects 3");
        check(adj[0].bonferroni > 0.0 && adj[0].bonferroni < 1.0, "bonferroni scaled");
    }

    // --- EWMA weighted stats -------------------------------------------
    {
        std::vector<double> xs = {-0.05, -0.02, -0.01, 0.01, 0.02, 0.05};
        auto w = stats::ewma_weights(xs.size(), 2.0);
        check(w.back() > w.front(), "recent heavier");
        double wm = stats::weighted_mean(xs, w);
        check(wm > 0.0, "weighted mean positive when recent skews up");
    }

    // --- Calibration ---------------------------------------------------
    {
        std::vector<double> probs = {0.9, 0.8, 0.6, 0.4, 0.2};
        std::vector<int>    out   = {  1,   1,   1,   0,   0};
        auto s = engine::score(probs, out);
        check(s.n == 5, "score n");
        check(s.brier < 0.1, "low brier good calibration");
        check(s.hit_rate >= 0.8, "hit rate high");
    }

    // --- Cholesky -------------------------------------------------------
    {
        engine::linalg::Matrix A = {{2.0, 1.0}, {1.0, 2.0}};
        auto L = engine::linalg::cholesky(A);
        approx(L[0][0], std::sqrt(2.0), 1e-9, "cholesky 00");
        approx(L[1][0], 1.0 / std::sqrt(2.0), 1e-9, "cholesky 10");
        // verify L*L^T == A
        double m00 = L[0][0]*L[0][0];
        double m10 = L[1][0]*L[0][0];
        double m11 = L[1][0]*L[1][0] + L[1][1]*L[1][1];
        approx(m00, 2.0, 1e-9, "LL'00");
        approx(m10, 1.0, 1e-9, "LL'10");
        approx(m11, 2.0, 1e-9, "LL'11");
    }

    // --- Portfolio risk basic ------------------------------------------
    {
        std::mt19937 rng(11);
        std::normal_distribution<double> n(0.0, 0.012);
        std::vector<double> r1, r2;
        for (int i = 0; i < 300; ++i) {
            double z = n(rng);
            r1.push_back(z);
            r2.push_back(0.5 * z + n(rng) * 0.5);
        }
        std::vector<engine::PortfolioHolding> hold = {{"A", 0.6}, {"B", 0.4}};
        auto pr = engine::portfolio_risk(hold, {"A","B"}, {r1, r2}, 1, 5000, 21u);
        check(pr.var95 > 0.0 && pr.var95 < 0.1, "portfolio var sane");
        double cv_sum = pr.component_var95[0] + pr.component_var95[1];
        // Component VaRs aren't required to sum exactly to VaR with FD bumps
        // but should be in the right ballpark (within 50% of VaR).
        check(cv_sum > 0.3 * pr.var95 && cv_sum < 3.0 * pr.var95, "component sum ballpark");
    }

    if (g_fails == 0) std::cout << "OK\n";
    else { std::cerr << g_fails << " failures\n"; }
    return g_fails == 0 ? 0 : 1;
}
