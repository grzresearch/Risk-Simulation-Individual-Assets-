#pragma once

#include "models.h"

#include <string>
#include <vector>

namespace macro::data {

// Lightweight FRED client. Free API key required.
// Docs: https://fred.stlouisfed.org/docs/api/fred/
class FredClient {
public:
    FredClient(std::string api_key);

    // Fetch a value series (e.g. "CPIAUCSL" for CPI All Urban, "FEDFUNDS" for fed funds).
    // Returns time-ordered observations. `from` and `to` are inclusive YYYY-MM-DD.
    std::vector<HistoricalRelease>
    fetch_release_history(const std::string& series_id,
                          const std::string& from,
                          const std::string& to);

    // Fetch the upcoming release calendar entries for a given release id.
    // The /releases/dates endpoint returns scheduled release dates.
    // Empty vector if the API key is missing or the call fails.
    std::vector<MacroEvent>
    fetch_release_calendar(int release_id,
                           EventKind kind,
                           const std::string& from,
                           const std::string& to);

    bool has_key() const { return !api_key_.empty(); }

private:
    std::string api_key_;
    static constexpr const char* kBase = "https://api.stlouisfed.org/fred";
};

// Mapping of EventKind -> (series_id, release_id) used by the FRED client.
struct FredSeriesMap {
    std::string series_id;
    int         release_id;
};

FredSeriesMap fred_mapping(EventKind k);

} // namespace macro::data
