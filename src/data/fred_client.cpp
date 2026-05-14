#include "data/fred_client.h"

#include "util/http_client.h"
#include "util/logger.h"
#include "util/time_utils.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdlib>
#include <stdexcept>

namespace macro::data {

using nlohmann::json;

FredClient::FredClient(std::string api_key) : api_key_(std::move(api_key)) {}

FredSeriesMap fred_mapping(EventKind k) {
    // release_id values from FRED's /releases endpoint.
    switch (k) {
        case EventKind::CPI:               return {"CPIAUCSL", 10};   // CPI for All Urban Consumers
        case EventKind::CorePCE:           return {"PCEPILFE", 21};   // Personal Income & Outlays
        case EventKind::PPI:               return {"PPIACO",   46};   // Producer Price Index
        case EventKind::NonFarmPayrolls:   return {"PAYEMS",   50};   // Employment Situation
        case EventKind::UnemploymentRate:  return {"UNRATE",   50};
        case EventKind::FOMC:              return {"FEDFUNDS", 101};  // H.15 / FOMC
        case EventKind::GDP:               return {"GDPC1",    53};
        case EventKind::RetailSales:       return {"RSAFS",    61};
        case EventKind::ISM_Manufacturing: return {"MANEMP",   0};    // ISM not on FRED; fallback only
        case EventKind::ConsumerSentiment: return {"UMCSENT",  171};
        case EventKind::Unknown:           return {"", 0};
    }
    return {"", 0};
}

std::vector<HistoricalRelease>
FredClient::fetch_release_history(const std::string& series_id,
                                  const std::string& from,
                                  const std::string& to) {
    std::vector<HistoricalRelease> out;
    if (api_key_.empty() || series_id.empty()) return out;

    auto resp = http::get(std::string(kBase) + "/series/observations",
        {
            {"series_id", series_id},
            {"api_key", api_key_},
            {"file_type", "json"},
            {"observation_start", from},
            {"observation_end", to},
        });
    if (!resp.ok()) {
        log::warn("FRED observations failed for " + series_id +
                  " status=" + std::to_string(resp.status));
        return out;
    }
    try {
        json j = json::parse(resp.body);
        double prev = std::nan("");
        for (const auto& o : j["observations"]) {
            const std::string ds  = o.value("date", "");
            const std::string val = o.value("value", ".");
            if (ds.empty() || val == "." || val.empty()) continue;
            HistoricalRelease r;
            r.date     = time::parse_date(ds);
            r.actual   = std::stod(val);
            r.previous = std::isnan(prev) ? r.actual : prev;
            r.consensus = std::nan(""); // FRED does not publish consensus
            out.push_back(r);
            prev = r.actual;
        }
    } catch (const std::exception& e) {
        log::warn(std::string("FRED parse error: ") + e.what());
    }
    return out;
}

std::vector<MacroEvent>
FredClient::fetch_release_calendar(int release_id,
                                   EventKind kind,
                                   const std::string& from,
                                   const std::string& to) {
    std::vector<MacroEvent> out;
    if (api_key_.empty() || release_id <= 0) return out;

    auto resp = http::get(std::string(kBase) + "/release/dates",
        {
            {"release_id", std::to_string(release_id)},
            {"api_key",    api_key_},
            {"file_type",  "json"},
            {"realtime_start", from},
            {"realtime_end",   to},
            {"include_release_dates_with_no_data", "true"},
        });
    if (!resp.ok()) {
        log::warn("FRED calendar failed for release_id=" + std::to_string(release_id));
        return out;
    }
    try {
        json j = json::parse(resp.body);
        for (const auto& r : j.value("release_dates", json::array())) {
            MacroEvent ev;
            ev.kind   = kind;
            ev.label  = to_string(kind);
            ev.date   = time::parse_date(r.value("date", ""));
            ev.source = "FRED release " + std::to_string(release_id);
            out.push_back(ev);
        }
    } catch (const std::exception& e) {
        log::warn(std::string("FRED calendar parse: ") + e.what());
    }
    return out;
}

} // namespace macro::data
