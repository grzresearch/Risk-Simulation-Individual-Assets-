#include "data/yahoo_client.h"

#include "util/http_client.h"
#include "util/logger.h"
#include "util/time_utils.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <stdexcept>

namespace macro::data {

using nlohmann::json;

namespace {
long to_unix(const std::string& iso) {
    auto tp = time::parse_date(iso);
    return static_cast<long>(std::chrono::system_clock::to_time_t(tp));
}
} // namespace

PriceSeries YahooClient::fetch_daily(const std::string& symbol,
                                     const std::string& from,
                                     const std::string& to) {
    PriceSeries out;
    if (symbol.empty()) return out;

    auto resp = http::get("https://query1.finance.yahoo.com/v8/finance/chart/" +
                          http::url_encode(symbol),
        {
            {"period1",  std::to_string(to_unix(from))},
            {"period2",  std::to_string(to_unix(to))},
            {"interval", "1d"},
            {"events",   "history"},
            {"includeAdjustedClose", "true"},
        },
        {{"Accept", "application/json"}});

    if (!resp.ok()) {
        log::warn("Yahoo chart failed: " + symbol +
                  " status=" + std::to_string(resp.status));
        return out;
    }
    try {
        json j = json::parse(resp.body);
        auto& result = j["chart"]["result"];
        if (!result.is_array() || result.empty()) return out;
        const auto& r0 = result[0];
        const auto& ts = r0["timestamp"];
        const auto& adj = r0["indicators"]["adjclose"][0]["adjclose"];
        const auto& closes = r0["indicators"]["quote"][0]["close"];
        const size_t n = ts.size();
        out.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            if (i >= adj.size() && i >= closes.size()) continue;
            double v = (i < adj.size() && !adj[i].is_null()) ? adj[i].get<double>()
                     : (i < closes.size() && !closes[i].is_null()) ? closes[i].get<double>() : 0.0;
            if (v <= 0.0) continue;
            PricePoint p;
            p.date  = std::chrono::system_clock::from_time_t(ts[i].get<long>());
            p.close = v;
            out.push_back(p);
        }
    } catch (const std::exception& e) {
        log::warn(std::string("Yahoo parse: ") + e.what());
    }
    return out;
}

double YahooClient::fetch_spot(const std::string& symbol) {
    if (symbol.empty()) return 0.0;
    auto resp = http::get("https://query1.finance.yahoo.com/v8/finance/chart/" +
                          http::url_encode(symbol),
        {{"interval","1d"}, {"range","5d"}});
    if (!resp.ok()) return 0.0;
    try {
        json j = json::parse(resp.body);
        auto& r0 = j["chart"]["result"][0];
        if (r0.contains("meta") && r0["meta"].contains("regularMarketPrice")) {
            return r0["meta"]["regularMarketPrice"].get<double>();
        }
    } catch (...) {}
    return 0.0;
}

} // namespace macro::data
