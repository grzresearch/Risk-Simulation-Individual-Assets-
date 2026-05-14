#pragma once

#include "models.h"

#include <string>

namespace macro::data {

// CoinGecko free tier: no key required, rate limited ~30 calls/min.
class CoinGeckoClient {
public:
    PriceSeries fetch_daily(const std::string& coin_id,
                            const std::string& from_yyyy_mm_dd,
                            const std::string& to_yyyy_mm_dd);

    double fetch_spot(const std::string& coin_id);
};

} // namespace macro::data
