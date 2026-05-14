#include "governance/model_card.h"

#include "util/time_utils.h"
#include "util/version.h"

#include <iomanip>
#include <ostream>
#include <sstream>

namespace macro::governance {

namespace {
std::string fmt(double v, int p = 4) {
    std::ostringstream o; o << std::fixed << std::setprecision(p) << v;
    return o.str();
}
} // namespace

void write_model_card(std::ostream& o, const Config& cfg, const engine::ScanOutput& s) {
    o << "# macro_risk — Model Card\n\n"
      << "**Version:** " << macro::version() << "  \n"
      << "**Git SHA:** " << macro::git_sha() << "  \n"
      << "**Build date:** " << macro::build_date() << "  \n"
      << "**Generated:** " << macro::time::to_iso(macro::time::today_utc()) << "  \n\n"
      << "## 1. Intended use\n\n"
      << "Provide directional probability and risk forecasts for individual\n"
      << "tradable assets (equity / ETF / bond / crypto / commodity / FX) over\n"
      << "the days following pre-scheduled macroeconomic releases. Not a\n"
      << "stand-alone trading signal; intended as an input to discretionary\n"
      << "risk and positioning decisions.\n\n"
      << "## 2. Inputs\n\n"
      << "- Macro time series and release calendar: FRED (St. Louis Fed).\n"
      << "- Daily prices: Yahoo Finance, CoinGecko.\n"
      << "- Optional consensus / surprise feeds: local CSV under `<cache>/consensus`.\n\n"
      << "## 3. Configuration of this run\n\n"
      << "- Horizon (days lookahead): " << cfg.horizon_days << "\n"
      << "- History years: " << cfg.history_years << "\n"
      << "- Monte-Carlo paths: " << cfg.mc_paths << "\n"
      << "- EWMA half-life (samples): " << cfg.ewma_half_life_samples << "\n"
      << "- FDR alpha: " << cfg.fdr_alpha << "\n"
      << "- Regime conditioning: " << (cfg.enable_regime_conditioning ? "on" : "off") << "\n"
      << "- GARCH(1,1): " << (cfg.enable_garch ? "on" : "off") << "\n"
      << "- Portfolio risk: " << (cfg.enable_portfolio_risk ? "on" : "off") << "\n"
      << "- Stress replays: " << (cfg.enable_stress_tests ? "on" : "off") << "\n"
      << "- Walk-forward backtest: " << (cfg.enable_backtest ? "on" : "off") << "\n\n"
      << "## 4. Methodology summary\n\n"
      << "For each upcoming macro event and each asset, the engine pulls every\n"
      << "historical release of the same kind, computes the asset's log return\n"
      << "over D+1 / D+5 / D+20 windows around each historical release, and\n"
      << "uses the resulting empirical conditional return distribution to:\n\n"
      << "1. Estimate P(up) (Jeffreys posterior).  \n"
      << "2. Reweight with an EWMA half-life to emphasise recent history.  \n"
      << "3. Sub-sample by current macro regime (CPI YoY, Fed Funds, Unemployment,\n"
      << "   VIX, SPY drawdown).  \n"
      << "4. Compute empirical 68% / 95% CI, VaR, ES.  \n"
      << "5. Project a Monte-Carlo terminal-price band mixing the event-conditional\n"
      << "   shock with bootstrap baseline drift.  \n"
      << "6. Estimate GARCH(1,1) implied 1-day sigma.  \n"
      << "7. Compute portfolio VaR/CVaR with Cholesky-decomposed empirical\n"
      << "   covariance plus finite-difference component contributions.  \n"
      << "8. Run a walk-forward back-test on the same forecast logic.  \n"
      << "9. Apply Bonferroni and Benjamini-Hochberg corrections across the\n"
      << "   full (asset x event) grid.\n\n"
      << "## 5. Validation evidence\n\n";

    if (s.assets.empty()) {
        o << "_No assets in the supplied sample output; rerun with a populated scan._\n\n";
    } else {
        o << "Walk-forward back-test (per asset, per event kind):\n\n"
          << "| Asset | Event | N | Brier | LogLoss | Hit | Sharpe | MaxDD |\n"
          << "|-------|-------|---|-------|---------|-----|--------|-------|\n";
        for (const auto& a : s.assets) {
            for (const auto& [name, bt] : a.backtests) {
                o << "| " << a.asset.id << " | " << name
                  << " | " << bt.n_events
                  << " | " << fmt(bt.brier, 3)
                  << " | " << fmt(bt.log_loss, 3)
                  << " | " << fmt(bt.hit_rate * 100, 1) << "%"
                  << " | " << fmt(bt.pnl_sharpe, 2)
                  << " | " << fmt(bt.max_drawdown * 100, 1) << "% |\n";
            }
        }
        o << "\n";
    }

    o << "## 6. Limitations & known issues\n\n"
      << "- Sample sizes per (asset, event) are typically 50-150; CIs are wide.\n"
      << "- FRED does not publish analyst consensus; standardised surprise is\n"
      << "  only computed where the local consensus CSV provides it.\n"
      << "- Daily resolution misses intra-day reaction asymmetries.\n"
      << "- Regime classifier is rule-based, not Markov-switching.\n"
      << "- Component-VaR uses finite differences; for production-grade risk\n"
      << "  attribution use Euler / conditional-expectation contributions.\n"
      << "- Yahoo Finance is unstable; production deployments should swap it\n"
      << "  for a contractual provider (Bloomberg, Refinitiv).\n"
      << "- No options / implied vol inputs; vol forecast is purely historical.\n\n"
      << "## 7. Reproducibility\n\n"
      << "- Engine version: " << macro::version() << "  \n"
      << "- Git commit: " << macro::git_sha() << "  \n"
      << "- Build date: " << macro::build_date() << "  \n"
      << "- Random seeds are deterministic functions of asset id, event kind,\n"
      << "  and event date.  \n"
      << "- All raw responses are cached on disk under `cache_dir`.\n\n";
}

} // namespace macro::governance
