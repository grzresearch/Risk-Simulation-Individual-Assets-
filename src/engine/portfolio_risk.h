#pragma once

#include "engine/linalg.h"
#include "models.h"

#include <string>
#include <vector>

namespace macro::engine {

struct PortfolioHolding {
    std::string asset_id;
    double      weight = 0.0; // signed exposure (1.0 = 100% long, -0.5 = short)
};

struct PortfolioRiskReport {
    int    n_assets = 0;
    double horizon_days = 1.0;
    double mean_return = 0.0;
    double stddev = 0.0;
    double var95 = 0.0;
    double var99 = 0.0;
    double es95  = 0.0;
    double es99  = 0.0;
    std::vector<std::string> asset_ids;
    std::vector<double>      weights;
    std::vector<double>      component_var95;  // sum ~ portfolio_var95
    std::vector<double>      marginal_var95;
    std::vector<std::string> correl_labels;
    linalg::Matrix           correlation;
};

// Inputs: aligned daily log-return vectors per asset.
PortfolioRiskReport
portfolio_risk(const std::vector<PortfolioHolding>& holdings,
               const std::vector<std::string>& asset_ids,
               const std::vector<std::vector<double>>& aligned_returns,
               int horizon_days = 1,
               int mc_paths = 50000,
               unsigned seed = 42);

} // namespace macro::engine
