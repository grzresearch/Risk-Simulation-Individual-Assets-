#include "data/price_repository.h"

#include "util/logger.h"
#include "util/time_utils.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>

namespace macro::data {

using nlohmann::json;
namespace fs = std::filesystem;

PriceRepository::PriceRepository(YahooClient& y, CoinGeckoClient& c, std::string cache_dir)
    : yahoo_(y), cg_(c), cache_dir_(std::move(cache_dir)) {
    if (!cache_dir_.empty()) {
        std::error_code ec;
        fs::create_directories(cache_dir_, ec);
    }
}

PriceSeries PriceRepository::load_cached(const std::string& key) const {
    PriceSeries out;
    if (cache_dir_.empty()) return out;
    auto path = fs::path(cache_dir_) / (key + ".json");
    std::ifstream f(path);
    if (!f) return out;
    try {
        json j; f >> j;
        for (const auto& p : j) {
            PricePoint pt;
            pt.date  = time::parse_date(p["d"].get<std::string>());
            pt.close = p["c"].get<double>();
            out.push_back(pt);
        }
    } catch (...) { out.clear(); }
    return out;
}

void PriceRepository::save_cache(const std::string& key, const PriceSeries& s) const {
    if (cache_dir_.empty() || s.empty()) return;
    auto path = fs::path(cache_dir_) / (key + ".json");
    json j = json::array();
    for (const auto& p : s) {
        j.push_back({{"d", time::to_iso(p.date)}, {"c", p.close}});
    }
    std::ofstream f(path);
    f << j.dump();
}

const PriceSeries& PriceRepository::history(const Asset& a,
                                            const std::string& from_iso,
                                            const std::string& to_iso) {
    const std::string key = a.id + "_" + from_iso + "_" + to_iso;
    auto it = cache_.find(key);
    if (it != cache_.end()) return it->second;

    PriceSeries s = load_cached(key);
    if (s.empty()) {
        if (a.asset_class == AssetClass::Crypto && !a.coingecko_id.empty()) {
            s = cg_.fetch_daily(a.coingecko_id, from_iso, to_iso);
        }
        if (s.empty() && !a.yahoo_symbol.empty()) {
            s = yahoo_.fetch_daily(a.yahoo_symbol, from_iso, to_iso);
        }
        if (!s.empty()) save_cache(key, s);
    }
    log::info("history " + a.id + ": " + std::to_string(s.size()) + " points");
    auto [ins, _] = cache_.emplace(key, std::move(s));
    return ins->second;
}

double PriceRepository::spot(const Asset& a) {
    if (a.asset_class == AssetClass::Crypto && !a.coingecko_id.empty()) {
        double p = cg_.fetch_spot(a.coingecko_id);
        if (p > 0.0) return p;
    }
    if (!a.yahoo_symbol.empty()) {
        double p = yahoo_.fetch_spot(a.yahoo_symbol);
        if (p > 0.0) return p;
    }
    return 0.0;
}

} // namespace macro::data
