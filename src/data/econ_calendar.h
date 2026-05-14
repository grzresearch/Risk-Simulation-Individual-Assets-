#pragma once

#include "data/fred_client.h"
#include "models.h"

#include <vector>

namespace macro::data {

// Aggregates upcoming events for the requested EventKinds within [from, to].
// Primary source is FRED's /release/dates endpoint. Falls back to a
// rule-based extrapolator (typical monthly release cadence) when no API key
// is available or the API call fails.
class EconCalendar {
public:
    explicit EconCalendar(FredClient& fred) : fred_(fred) {}

    std::vector<MacroEvent>
    upcoming(const std::vector<EventKind>& kinds,
             const std::string& from_iso,
             const std::string& to_iso);

private:
    FredClient& fred_;
    std::vector<MacroEvent> fallback(EventKind k,
                                     const std::string& from,
                                     const std::string& to);
};

} // namespace macro::data
