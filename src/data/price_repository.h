#pragma once

#include "data/coingecko_client.h"
#include "data/yahoo_client.h"
#include "models.h"

#include <string>
#include <unordered_map>

namespace macro::data {

// Caches price series by asset id. Picks the right provider per asset class.
class PriceRepository {
public:
    PriceRepository(YahooClient& y, CoinGeckoClient& c, std::string cache_dir);

    const PriceSeries& history(const Asset& a,
                               const std::string& from_iso,
                               const std::string& to_iso);

    double spot(const Asset& a);

private:
    YahooClient&        yahoo_;
    CoinGeckoClient&    cg_;
    std::string         cache_dir_;
    std::unordered_map<std::string, PriceSeries> cache_;

    PriceSeries load_cached(const std::string& key) const;
    void        save_cache(const std::string& key, const PriceSeries& s) const;
};

} // namespace macro::data
