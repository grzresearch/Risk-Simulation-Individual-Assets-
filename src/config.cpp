#include "config.h"

#include "util/logger.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>

namespace macro {

using nlohmann::json;

Config load_config(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("cannot open config: " + path);
    json j; f >> j;

    Config cfg;
    if (j.contains("api_keys")) {
        cfg.api_keys.fred         = j["api_keys"].value("fred", "");
        cfg.api_keys.alphavantage = j["api_keys"].value("alphavantage", "");
    }
    cfg.horizon_days   = j.value("horizon_days", 90);
    cfg.history_years  = j.value("history_years", 12);
    cfg.mc_paths       = j.value("mc_paths", 10000);
    cfg.offline        = j.value("offline", false);
    cfg.cache_dir      = j.value("cache_dir", std::string(".cache"));

    if (j.contains("return_horizons")) {
        cfg.return_horizons.clear();
        for (auto& v : j["return_horizons"]) cfg.return_horizons.push_back(v.get<int>());
    }

    if (j.contains("events")) {
        for (auto& v : j["events"]) {
            cfg.events.push_back(parse_event_kind(v.get<std::string>()));
        }
    } else {
        cfg.events = {EventKind::CPI, EventKind::FOMC, EventKind::NonFarmPayrolls,
                      EventKind::PPI, EventKind::CorePCE, EventKind::UnemploymentRate};
    }

    if (!j.contains("assets") || !j["assets"].is_array()) {
        throw std::runtime_error("config: missing 'assets' array");
    }
    for (auto& a : j["assets"]) {
        Asset asset;
        asset.id            = a.value("id", "");
        asset.display_name  = a.value("name", asset.id);
        asset.asset_class   = parse_asset_class(a.value("class", "unknown"));
        asset.yahoo_symbol  = a.value("yahoo", "");
        asset.coingecko_id  = a.value("coingecko", "");
        asset.fred_series   = a.value("fred", "");
        if (asset.id.empty()) {
            throw std::runtime_error("config: asset missing 'id'");
        }
        cfg.assets.push_back(std::move(asset));
    }
    log::info("Loaded config: " + std::to_string(cfg.assets.size()) +
              " assets, " + std::to_string(cfg.events.size()) + " event kinds");
    return cfg;
}

void write_default_config(const std::string& path) {
    json j = {
        {"api_keys", {
            {"fred", "REPLACE_WITH_FRED_KEY"},
            {"alphavantage", ""}
        }},
        {"horizon_days", 90},
        {"history_years", 12},
        {"mc_paths", 10000},
        {"offline", false},
        {"return_horizons", {1, 5, 20}},
        {"events", {"CPI", "FOMC", "NFP", "PPI", "PCE", "UNEMPLOYMENT"}},
        {"assets", json::array({
            {{"id","aapl"}, {"name","Apple Inc."},  {"class","equity"},    {"yahoo","AAPL"}},
            {{"id","spy"},  {"name","SPDR S&P 500"},{"class","etf"},       {"yahoo","SPY"}},
            {{"id","tlt"},  {"name","20Y Treasury ETF"},{"class","bond"},  {"yahoo","TLT"}},
            {{"id","btc"},  {"name","Bitcoin"},     {"class","crypto"},    {"coingecko","bitcoin"}, {"yahoo","BTC-USD"}},
            {{"id","eth"},  {"name","Ethereum"},    {"class","crypto"},    {"coingecko","ethereum"}, {"yahoo","ETH-USD"}},
            {{"id","gld"},  {"name","Gold ETF"},    {"class","commodity"}, {"yahoo","GLD"}},
            {{"id","cl"},   {"name","WTI Crude"},   {"class","commodity"}, {"yahoo","CL=F"}}
        })}
    };
    std::ofstream f(path);
    f << j.dump(2);
}

} // namespace macro
