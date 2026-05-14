#pragma once

#include "config.h"
#include "data/consensus_provider.h"
#include "data/econ_calendar.h"
#include "data/fred_client.h"
#include "data/price_repository.h"
#include "models.h"
#include "util/holiday_calendar.h"

#include <vector>

namespace macro::engine {

struct ScanOutput {
    std::vector<AssetReport> assets;
    PortfolioSnapshot        portfolio;
};

class Scanner {
public:
    Scanner(const Config& cfg,
            data::FredClient& fred,
            data::EconCalendar& calendar,
            data::PriceRepository& prices,
            data::ConsensusProvider& consensus);

    ScanOutput run();

private:
    const Config& cfg_;
    data::FredClient&        fred_;
    data::EconCalendar&      cal_;
    data::PriceRepository&   prices_;
    data::ConsensusProvider& consensus_;
    time::HolidayCalendar    holidays_;

    ScenarioResult build_scenario(const Asset& a,
                                  const MacroEvent& ev,
                                  const PriceSeries& history,
                                  const std::vector<HistoricalRelease>& releases,
                                  double spot,
                                  double garch_sigma_1d,
                                  const std::vector<double>& baseline_returns,
                                  const std::vector<double>& regime_filtered_returns,
                                  const std::string& current_regime);

    void apply_multi_testing(std::vector<AssetReport>& reports);

    PortfolioSnapshot build_portfolio_snapshot(
        const std::vector<AssetReport>& reports,
        const std::vector<Asset>& assets,
        std::map<std::string, std::vector<double>>& asset_returns_aligned);
};

} // namespace macro::engine
