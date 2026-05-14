#pragma once

#include "config.h"
#include "data/econ_calendar.h"
#include "data/fred_client.h"
#include "data/price_repository.h"
#include "models.h"

#include <vector>

namespace macro::engine {

// Orchestrates: load history -> match past releases -> compute reactions ->
// project upcoming events. Returns one AssetReport per asset.
class Scanner {
public:
    Scanner(const Config& cfg,
            data::FredClient& fred,
            data::EconCalendar& calendar,
            data::PriceRepository& prices);

    std::vector<AssetReport> run();

private:
    const Config& cfg_;
    data::FredClient&      fred_;
    data::EconCalendar&    cal_;
    data::PriceRepository& prices_;

    ScenarioResult build_scenario(const Asset& a,
                                  const MacroEvent& ev,
                                  const PriceSeries& history,
                                  const std::vector<HistoricalRelease>& releases,
                                  double spot,
                                  double daily_vol,
                                  const std::vector<double>& baseline_returns);
};

} // namespace macro::engine
