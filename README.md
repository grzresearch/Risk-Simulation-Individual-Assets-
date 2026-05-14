# macro_risk — Live Macro Driven Risk & Direction Engine

C++17 command-line tool that fetches **live macro data** (CPI, PPI, Core PCE,
NFP, FOMC, Unemployment, GDP, Retail Sales, ISM, UMich) and computes:

- **Direction probability** with empirical, EWMA-weighted, and regime-conditional
  Bayesian posteriors
- **68% / 95% confidence intervals**, VaR, ES
- **GARCH(1,1) volatility forecast** for each asset
- **Walk-forward backtest** scoring (Brier, log-loss, hit-rate, Sharpe, max-DD)
- **Multi-asset portfolio risk** (Cholesky MC, VaR/CVaR, component VaR)
- **Historical stress replays** (GFC, Covid, 2022 CPI shock, ...)
- **Multiple-testing correction** (Bonferroni + Benjamini-Hochberg FDR)

For every (asset x upcoming event) within a configurable horizon (default 90
days) it asks: *historically, how did this asset react in the days following
this exact event, in this regime?* and turns the empirical distribution into
a forecast that is calibrated against out-of-sample data.

---

## Data sources

| Provider     | Data                                | Key required |
|--------------|--------------------------------------|--------------|
| FRED         | Macro time series + release calendar | yes (free)   |
| Yahoo Finance| Daily OHLC for equities/ETFs/FX      | no           |
| CoinGecko    | Daily prices for crypto              | no           |
| Local CSVs   | Optional consensus / surprise feed   | no           |

Consensus CSVs are read from `<cache_dir>/consensus/<event>.csv`. Row format:
```
yyyy-mm-dd,consensus,actual,previous
2024-08-14,0.2,0.2,0.3
```
The engine falls back to a rule-based monthly calendar if FRED is unreachable.

---

## What the engine produces

### Per asset / per upcoming event
- `prob_up` (empirical Bayesian posterior with Jeffreys prior)
- `weighted_prob_up` (EWMA-weighted, recent events count more, default half-life
  24 samples)
- `regime_prob_up` (conditional on the current regime: Easing / Hiking /
  Stagflation / Risk-Off / Neutral)
- `expected_surprise_z` (standardised surprise expectation given consensus
  vs previous)
- `bonferroni_p`, `bh_q` (multiple-testing-corrected significance)
- `surprise_beta` (OLS slope of return on release surprise when consensus
  is available)
- `mc_band` p05 / p50 / p95 terminal price after the horizon
- `garch_sigma_1d` (GARCH(1,1) implied 1-day sigma)

### Per asset
- `hist_vol_annualized` (sample), `garch_vol_annualized` (GARCH-fit)
- Walk-forward backtest per event kind:
  `n_events`, `brier`, `log_loss`, `hit_rate`, `pnl_total`, `pnl_sharpe`,
  `pnl_sortino`, `max_drawdown`
- Stress replays across 9 historical macro shocks

### Portfolio (if `portfolio` set in config)
- Multi-asset Cholesky-based MC over the aligned daily return matrix
- 1-day `VaR95 / VaR99 / ES95 / ES99`
- `marginal_var95` and `component_var95` per holding
- Empirical correlation matrix

---

## Build

Requirements: CMake >= 3.16, C++17 compiler, `libcurl`.
`nlohmann/json` is auto-fetched via `FetchContent`.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# unit tests
cmake -S . -B build -DBUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Run

```bash
export FRED_API_KEY=your_key_here     # free at fred.stlouisfed.org
./build/macro_risk --init my_config.json
./build/macro_risk --config my_config.json
./build/macro_risk --config my_config.json --json > report.json
```

---

## Pipeline

1. **Load** the asset universe + macro event list + portfolio + knobs.
2. **Pull upcoming events** within the horizon from FRED `/release/dates`.
3. **Pull historical releases** per event kind from FRED.
4. **Enrich** with consensus CSVs (if present in `<cache_dir>/consensus`).
5. **Pull daily price history** per asset from Yahoo / CoinGecko (cached on disk).
6. **Build a regime history** day-by-day from CPI YoY, Fed Funds, Unemployment
   and SPY 3-month drawdown.
7. **Fit GARCH(1,1)** per asset on log returns (fallback to RiskMetrics on
   convergence failure).
8. **Per (asset, upcoming event)**: align history to historical release dates,
   compute D+1/+5/+20 log returns and:
   - empirical posterior P(up), 68/95 CIs, VaR/ES
   - EWMA-weighted variants
   - regime-conditional sub-sample
   - standardised expected surprise
   - empirical-mixture Monte-Carlo price band
9. **Multiple-testing**: apply Bonferroni and Benjamini-Hochberg FDR across
   the full (asset x event) grid of sign-test p-values.
10. **Walk-forward backtest** per (asset, event kind) using only data strictly
    before each event; score Brier, log-loss, hit-rate, and run a sign-sized
    long/short PnL strategy on the forecast.
11. **Replay historical stress scenarios** against each asset's price path.
12. **Portfolio risk** via Cholesky on the empirical covariance matrix +
    Monte-Carlo, with finite-difference component-VaR.
13. **Report** as a text table or machine-readable JSON.

---

## Statistical notes

- **EWMA weights**: `w_i = exp(-ln(2) * age_i / half_life)`. Set
  `ewma_half_life_samples` to a small number (e.g. 12) to put more weight on
  the most recent regime; a larger value (e.g. 60) flattens it.
- **Regime classification** is rule-based (priority-ordered thresholds on CPI
  YoY, Fed Funds, Unemployment, VIX, SPY drawdown). For a full HMM/Markov-
  switching model swap `regime.cpp` for a state-space estimator.
- **Multiple testing** uses Benjamini-Hochberg with `fdr_alpha` (default 5%).
  Rows where `bh_q < fdr_alpha` are the FDR-significant findings.
- **Calibration**: Brier-score is the mean squared distance between forecast
  P(up) and the realised binary outcome; log-loss is the negative log-
  likelihood. Lower is better. A naive 50/50 forecaster scores 0.25 Brier.
- **GARCH(1,1)** is fit via a 3-D Nelder-Mead simplex on the Gaussian log-
  likelihood with feasibility penalties. Convergence falls back to
  RiskMetrics (alpha=0.06, beta=0.94) if the MLE diverges.
- **Component-VaR** is computed by finite-difference: bump each weight by 1%,
  re-simulate, divide. It is approximate; for production switch to
  conditional-expectation contributions.
- **Stress replays** apply the asset's *own* price path through the windowed
  period -- not a synthetic shock. Therefore a name that did not yet trade
  during, e.g., the GFC will simply have no GFC entry.

---

## Project layout

```
src/
  util/              http_client, time_utils, logger, holiday_calendar
  data/              FRED, Yahoo, CoinGecko, econ-calendar, price cache,
                     consensus provider
  engine/            historical_analyzer, statistical_engine, monte_carlo,
                     risk_calculator, surprise, garch, regime, multitest,
                     calibration, linalg, portfolio_risk, stress, backtest,
                     scanner
  report/            text + JSON output
  main.cpp           CLI
tests/               statistics + advanced (GARCH/regime/FDR/portfolio) tests
config.example.json  template asset universe + portfolio
```

## License

MIT.
