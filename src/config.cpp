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
    cfg.ewma_half_life_samples = j.value("ewma_half_life_samples", 24.0);
    cfg.fdr_alpha              = j.value("fdr_alpha", 0.05);
    cfg.enable_regime_conditioning = j.value("enable_regime_conditioning", true);
    cfg.enable_garch               = j.value("enable_garch", true);
    cfg.enable_portfolio_risk      = j.value("enable_portfolio_risk", true);
    cfg.enable_stress_tests        = j.value("enable_stress_tests", true);
    cfg.enable_backtest            = j.value("enable_backtest", true);
    cfg.backtest_min_train         = j.value("backtest_min_train", 24);

    if (j.contains("portfolio") && j["portfolio"].is_array()) {
        for (const auto& h : j["portfolio"]) {
            PortfolioHoldingCfg pc;
            pc.asset_id = h.value("id", "");
            pc.weight   = h.value("weight", 0.0);
            if (!pc.asset_id.empty()) cfg.portfolio.push_back(pc);
        }
    }

    if (j.contains("server")) {
        const auto& s = j["server"];
        cfg.server.enabled   = s.value("enabled", false);
        cfg.server.host      = s.value("host", std::string("0.0.0.0"));
        cfg.server.port      = s.value("port", 8080);
        cfg.server.api_token = s.value("api_token", std::string(""));
    }
    if (j.contains("alerts")) {
        const auto& a = j["alerts"];
        cfg.alerts.enabled         = a.value("enabled", false);
        cfg.alerts.slack_webhook   = a.value("slack_webhook", std::string(""));
        cfg.alerts.prob_threshold  = a.value("prob_threshold", 0.20);
        cfg.alerts.bh_q_threshold  = a.value("bh_q_threshold", 0.10);
        cfg.alerts.min_sample_size = a.value("min_sample_size", 20);
    }
    if (j.contains("persistence")) {
        const auto& p = j["persistence"];
        cfg.persist.backend      = p.value("backend", std::string("json"));
        cfg.persist.json_dir     = p.value("json_dir", std::string(".cache/runs"));
        cfg.persist.postgres_dsn = p.value("postgres_dsn", std::string(""));
    }
    if (j.contains("audit")) {
        const auto& a = j["audit"];
        cfg.audit.enabled = a.value("enabled", true);
        cfg.audit.path    = a.value("path", std::string(".cache/audit.log"));
        cfg.audit.actor   = a.value("actor", std::string("system"));
    }
    cfg.structured_logging = j.value("structured_logging", false);
    cfg.worker_threads     = j.value("worker_threads", 0);

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
        {"mc_paths", 20000},
        {"offline", false},
        {"return_horizons", {1, 5, 20}},
        {"ewma_half_life_samples", 24},
        {"fdr_alpha", 0.05},
        {"enable_regime_conditioning", true},
        {"enable_garch", true},
        {"enable_portfolio_risk", true},
        {"enable_stress_tests", true},
        {"enable_backtest", true},
        {"backtest_min_train", 24},
        {"events", {"CPI", "FOMC", "NFP", "PPI", "PCE", "UNEMPLOYMENT"}},
        {"assets", json::array({
            {{"id","aapl"}, {"name","Apple Inc."},  {"class","equity"},    {"yahoo","AAPL"}},
            {{"id","spy"},  {"name","SPDR S&P 500"},{"class","etf"},       {"yahoo","SPY"}},
            {{"id","tlt"},  {"name","20Y Treasury ETF"},{"class","bond"},  {"yahoo","TLT"}},
            {{"id","btc"},  {"name","Bitcoin"},     {"class","crypto"},    {"coingecko","bitcoin"}, {"yahoo","BTC-USD"}},
            {{"id","eth"},  {"name","Ethereum"},    {"class","crypto"},    {"coingecko","ethereum"}, {"yahoo","ETH-USD"}},
            {{"id","gld"},  {"name","Gold ETF"},    {"class","commodity"}, {"yahoo","GLD"}},
            {{"id","cl"},   {"name","WTI Crude"},   {"class","commodity"}, {"yahoo","CL=F"}}
        })},
        {"portfolio", json::array({
            {{"id","spy"}, {"weight", 0.5}},
            {{"id","tlt"}, {"weight", 0.3}},
            {{"id","gld"}, {"weight", 0.1}},
            {{"id","btc"}, {"weight", 0.1}}
        })},
        {"structured_logging", false},
        {"worker_threads", 0},
        {"server", {
            {"enabled", false},
            {"host", "0.0.0.0"},
            {"port", 8080},
            {"api_token", ""}
        }},
        {"alerts", {
            {"enabled", false},
            {"slack_webhook", ""},
            {"prob_threshold", 0.20},
            {"bh_q_threshold", 0.10},
            {"min_sample_size", 20}
        }},
        {"persistence", {
            {"backend", "json"},
            {"json_dir", ".cache/runs"},
            {"postgres_dsn", ""}
        }},
        {"audit", {
            {"enabled", true},
            {"path", ".cache/audit.log"},
            {"actor", "system"}
        }}
    };
    std::ofstream f(path);
    f << j.dump(2);
}

} // namespace macro
