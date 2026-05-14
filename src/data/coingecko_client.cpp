#include "data/coingecko_client.h"

#include "util/http_client.h"
#include "util/logger.h"
#include "util/time_utils.h"

#include <nlohmann/json.hpp>

#include <chrono>

namespace macro::data {

using nlohmann::json;

namespace {
long to_unix(const std::string& iso) {
    auto tp = time::parse_date(iso);
    return static_cast<long>(std::chrono::system_clock::to_time_t(tp));
}
} // namespace

PriceSeries CoinGeckoClient::fetch_daily(const std::string& coin_id,
                                         const std::string& from,
                                         const std::string& to) {
    PriceSeries out;
    if (coin_id.empty()) return out;

    auto resp = http::get(
        "https://api.coingecko.com/api/v3/coins/" + http::url_encode(coin_id) +
            "/market_chart/range",
        {
            {"vs_currency", "usd"},
            {"from", std::to_string(to_unix(from))},
            {"to",   std::to_string(to_unix(to))},
        });
    if (!resp.ok()) {
        log::warn("CoinGecko failed: " + coin_id +
                  " status=" + std::to_string(resp.status));
        return out;
    }
    try {
        json j = json::parse(resp.body);
        for (const auto& pair : j["prices"]) {
            if (pair.size() < 2) continue;
            long ms = pair[0].get<long>();
            PricePoint p;
            p.date  = std::chrono::system_clock::from_time_t(ms / 1000);
            p.close = pair[1].get<double>();
            out.push_back(p);
        }
    } catch (const std::exception& e) {
        log::warn(std::string("CoinGecko parse: ") + e.what());
    }
    return out;
}

double CoinGeckoClient::fetch_spot(const std::string& coin_id) {
    if (coin_id.empty()) return 0.0;
    auto resp = http::get("https://api.coingecko.com/api/v3/simple/price",
        {{"ids", coin_id}, {"vs_currencies","usd"}});
    if (!resp.ok()) return 0.0;
    try {
        json j = json::parse(resp.body);
        return j[coin_id]["usd"].get<double>();
    } catch (...) {}
    return 0.0;
}

} // namespace macro::data
