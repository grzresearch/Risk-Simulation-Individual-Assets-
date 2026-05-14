#include "models.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace macro {

namespace {
std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}
} // namespace

const char* to_string(AssetClass c) {
    switch (c) {
        case AssetClass::Equity:    return "equity";
        case AssetClass::ETF:       return "etf";
        case AssetClass::Bond:      return "bond";
        case AssetClass::Crypto:    return "crypto";
        case AssetClass::Commodity: return "commodity";
        case AssetClass::FX:        return "fx";
        case AssetClass::Index:     return "index";
        case AssetClass::Unknown:   return "unknown";
    }
    return "unknown";
}

AssetClass parse_asset_class(const std::string& s) {
    const std::string l = to_lower(s);
    if (l == "equity" || l == "stock")     return AssetClass::Equity;
    if (l == "etf")                        return AssetClass::ETF;
    if (l == "bond")                       return AssetClass::Bond;
    if (l == "crypto" || l == "coin")      return AssetClass::Crypto;
    if (l == "commodity" || l == "comdty") return AssetClass::Commodity;
    if (l == "fx" || l == "forex")         return AssetClass::FX;
    if (l == "index")                      return AssetClass::Index;
    return AssetClass::Unknown;
}

const char* to_string(EventKind k) {
    switch (k) {
        case EventKind::CPI:               return "CPI";
        case EventKind::CorePCE:           return "Core PCE";
        case EventKind::PPI:               return "PPI";
        case EventKind::NonFarmPayrolls:   return "Non-Farm Payrolls";
        case EventKind::UnemploymentRate:  return "Unemployment Rate";
        case EventKind::FOMC:              return "FOMC Rate Decision";
        case EventKind::GDP:               return "GDP";
        case EventKind::RetailSales:       return "Retail Sales";
        case EventKind::ISM_Manufacturing: return "ISM Manufacturing PMI";
        case EventKind::ConsumerSentiment: return "Consumer Sentiment";
        case EventKind::Unknown:           return "Unknown";
    }
    return "Unknown";
}

EventKind parse_event_kind(const std::string& s) {
    const std::string l = to_lower(s);
    if (l == "cpi")            return EventKind::CPI;
    if (l == "core_pce" || l == "pce")  return EventKind::CorePCE;
    if (l == "ppi" || l == "vpi")       return EventKind::PPI; // VPI alias
    if (l == "nfp" || l == "payrolls")  return EventKind::NonFarmPayrolls;
    if (l == "unemployment")   return EventKind::UnemploymentRate;
    if (l == "fomc" || l == "rate")     return EventKind::FOMC;
    if (l == "gdp")            return EventKind::GDP;
    if (l == "retail_sales")   return EventKind::RetailSales;
    if (l == "ism")            return EventKind::ISM_Manufacturing;
    if (l == "sentiment" || l == "umich") return EventKind::ConsumerSentiment;
    return EventKind::Unknown;
}

double pct_change(double from, double to) {
    if (from == 0.0) return 0.0;
    return (to - from) / from;
}

} // namespace macro
