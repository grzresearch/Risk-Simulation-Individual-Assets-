# macro_risk – Live Macro Driven Risk & Direction Engine

C++17 command-line tool that fetches **live macro data** (CPI, PPI, Core PCE,
NFP, FOMC, Unemployment, GDP, Retail Sales, ISM, UMich) and computes the
**directional probability**, **confidence intervals**, **VaR / ES** and a
**Monte-Carlo price band** for each user-specified asset (equity, ETF, bond,
crypto, commodity, FX) across all macro events scheduled in the **next N
days** (default 90).

For every (asset × upcoming event) it asks: *historically, how did this asset
react in the days following this exact event?* and turns the empirical
distribution into a forecast.

## Data sources

| Provider     | Data                                | Key required |
|--------------|--------------------------------------|--------------|
| FRED         | Macro time series + release calendar | yes (free)   |
| Yahoo Finance| Daily OHLC for equities/ETFs/FX      | no           |
| CoinGecko    | Daily prices for crypto              | no           |

The release calendar falls back to deterministic monthly heuristics if FRED
is unreachable so the tool still produces output without a key.

## Build

Requirements: CMake >= 3.16, a C++17 compiler, `libcurl`, internet access for
`FetchContent` to pull `nlohmann/json` (or install it system-wide).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Optional unit tests:

```bash
cmake -S . -B build -DBUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Run

```bash
# 1. Get a free FRED key: https://fred.stlouisfed.org/docs/api/api_key.html
export FRED_API_KEY=your_key_here

# 2. Generate a config skeleton (or copy config.example.json)
./build/macro_risk --init my_config.json

# 3. Edit my_config.json: list the assets you care about

# 4. Run the scan
./build/macro_risk --config my_config.json
./build/macro_risk --config my_config.json --json > report.json
```

## What the engine does

1. **Load config** with the asset universe and macro event list.
2. **Pull upcoming events** within the horizon from FRED's `/release/dates`.
3. **Pull historical releases** (e.g. every CPI print since 2012) from FRED.
4. **Pull daily price history** per asset from Yahoo / CoinGecko (cached).
5. **Align**: for each historical release date, compute the asset's log return
   over D+1 / D+5 / D+20 windows (configurable).
6. **Build the conditional return sample** per (asset × event kind).
7. **Forecast each upcoming event**:
   - Posterior P(up) via Beta(0.5, 0.5) prior on observed sign counts.
   - 68% and 95% empirical confidence intervals.
   - Historical VaR_95 and Expected-Shortfall_95 (1-day equivalent).
   - Sign-test two-sided p-value for "median != 0".
   - OLS slope of return on release "surprise" (when consensus available).
   - Monte-Carlo mixture (event shock day 1, baseline empirical drift after)
     producing the p05 / p50 / p95 terminal-price band.
8. **Report** as a text table or JSON.

## Example output (truncated)

```
-- spy | SPDR S&P 500 (etf)
   Spot: 528.4100  |  Annualized vol: 14.32%
   Scenarios (5d horizon):
   Date         Event                 Pup       MeanRet     Median   95% CI                  VaR95   n     p(sgn)    Beta(surprise)
   2026-05-15   CPI                   58.33%    +0.34%      +0.42%   [-2.81%, +3.07%]        2.65%   72    0.087     -0.0023
   2026-05-15   FOMC Rate Decision    47.06%    -0.18%      -0.05%   [-3.50%, +2.95%]        3.20%   68    0.412     -
       MC band (D+5): p05=512.18  p50=529.07  p95=547.92
```

## Statistical notes

* The "surprise beta" regresses *log return* on *(actual - consensus)*.
  Consensus is generally not published by FRED, so the beta is only computed
  when the upstream data carries it.
* The Monte-Carlo uses a mixture model: with weight `w=1` the first day draws
  from the **event-conditional empirical** return distribution (no Gaussian
  assumption -- fat tails survive); subsequent days draw from the
  unconditional daily-return empirical distribution.
* All confidence intervals are **empirical / bootstrap based**, not
  parametric. They will narrow as your historical sample grows, so a longer
  `history_years` setting gives tighter intervals.
* The sign test is robust to outliers but underestimates strength when the
  distribution is heavily skewed. Cross-reference with the mean and beta.

## Project layout

```
src/
  util/              http_client, time_utils, logger
  data/              FRED, Yahoo, CoinGecko, economic calendar, price cache
  engine/            historical analyzer, statistics, Monte-Carlo, scanner
  report/            text + JSON output
  main.cpp           CLI
tests/               unit tests for the stats / risk math
config.example.json  template asset universe
```

## License

MIT.
