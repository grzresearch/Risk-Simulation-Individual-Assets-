#pragma once

#include "models.h"
#include "util/time_utils.h"

#include <string>
#include <vector>

namespace macro::engine {

struct StressScenario {
    std::string label;
    std::string description;
    time::Date  start;
    time::Date  end;
};

struct StressResult {
    StressScenario scenario;
    std::string    asset_id;
    double         total_return = 0.0;  // log return over the window
    double         max_drawdown = 0.0;  // negative number
    int            duration_days = 0;
};

// Library of canonical macro stress windows.
std::vector<StressScenario> default_scenarios();

// Replay a scenario against an asset's daily price history.
StressResult replay(const StressScenario& s,
                    const std::string& asset_id,
                    const PriceSeries& prices);

} // namespace macro::engine
