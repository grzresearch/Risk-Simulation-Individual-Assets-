#pragma once

#include "models.h"

#include <string>

namespace macro::data {

// Fetches OHLC history from Yahoo Finance's v8 chart endpoint.
// No API key required. Returns daily closes, sorted ascending.
class YahooClient {
public:
    PriceSeries fetch_daily(const std::string& symbol,
                            const std::string& from_yyyy_mm_dd,
                            const std::string& to_yyyy_mm_dd);

    double fetch_spot(const std::string& symbol);
};

} // namespace macro::data
