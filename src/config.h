#pragma once

#include "models.h"

#include <string>
#include <vector>

namespace macro {

struct ApiKeys {
    std::string fred;        // FRED API key (https://fred.stlouisfed.org/docs/api/api_key.html)
    std::string alphavantage; // optional fallback for prices
};

struct Config {
    ApiKeys            api_keys;
    std::vector<Asset> assets;
    std::vector<EventKind> events;     // events the user wants screened
    int                horizon_days = 90; // upcoming-events lookahead
    std::vector<int>   return_horizons = {1, 5, 20}; // D+N returns to study
    int                history_years = 12; // how far back to load history
    int                mc_paths = 10000;
    bool               offline = false; // skip network, use cached/json
    std::string        cache_dir = ".cache";
};

Config load_config(const std::string& path);
void   write_default_config(const std::string& path);

} // namespace macro
