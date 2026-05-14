#include "data/consensus_provider.h"

#include "util/logger.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace macro::data {

namespace fs = std::filesystem;

namespace {

std::string slug(EventKind k) {
    switch (k) {
        case EventKind::CPI:               return "cpi";
        case EventKind::CorePCE:           return "pce";
        case EventKind::PPI:               return "ppi";
        case EventKind::NonFarmPayrolls:   return "nfp";
        case EventKind::UnemploymentRate:  return "unemployment";
        case EventKind::FOMC:              return "fomc";
        case EventKind::GDP:               return "gdp";
        case EventKind::RetailSales:       return "retail_sales";
        case EventKind::ISM_Manufacturing: return "ism";
        case EventKind::ConsumerSentiment: return "umich";
        case EventKind::Unknown:           return "unknown";
    }
    return "unknown";
}

std::optional<double> parse_double(const std::string& s) {
    if (s.empty() || s == "NA" || s == "nan" || s == "-") return std::nullopt;
    try { return std::stod(s); } catch (...) { return std::nullopt; }
}

std::vector<std::string> split_csv(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : line) {
        if (c == ',') { out.push_back(cur); cur.clear(); }
        else if (c != '\r') cur.push_back(c);
    }
    out.push_back(cur);
    return out;
}

} // namespace

ConsensusProvider::ConsensusProvider(std::string base_dir)
    : base_dir_(std::move(base_dir)) {
    if (base_dir_.empty()) return;
    fs::path dir = fs::path(base_dir_) / "consensus";
    std::error_code ec;
    fs::create_directories(dir, ec);
}

void ConsensusProvider::load_kind(EventKind k) {
    if (tables_.count(static_cast<int>(k))) return;
    DateMap m;
    if (base_dir_.empty()) {
        tables_.emplace(static_cast<int>(k), std::move(m));
        return;
    }
    fs::path path = fs::path(base_dir_) / "consensus" / (slug(k) + ".csv");
    std::ifstream f(path);
    if (!f) {
        tables_.emplace(static_cast<int>(k), std::move(m));
        return;
    }
    std::string line;
    bool first = true;
    std::size_t loaded = 0;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        auto cols = split_csv(line);
        if (first) {
            first = false;
            // Skip header if it isn't a date.
            if (cols.empty() || cols[0].size() != 10 || cols[0][4] != '-') continue;
        }
        if (cols.empty()) continue;
        const std::string& d = cols[0];
        Row r;
        if (cols.size() > 1) r.consensus = parse_double(cols[1]);
        if (cols.size() > 2) r.actual    = parse_double(cols[2]);
        if (cols.size() > 3) r.previous  = parse_double(cols[3]);
        m[d] = r;
        ++loaded;
    }
    total_rows_ += loaded;
    log::info("Consensus[" + slug(k) + "]: " + std::to_string(loaded) + " rows from " + path.string());
    tables_.emplace(static_cast<int>(k), std::move(m));
}

std::optional<ConsensusProvider::Row>
ConsensusProvider::lookup(EventKind k, time::Date d) const {
    auto it = tables_.find(static_cast<int>(k));
    if (it == tables_.end()) return std::nullopt;
    const auto& m = it->second;
    // Exact match first
    auto exact = m.find(time::to_iso(d));
    if (exact != m.end()) return exact->second;
    // +/- 3 days tolerance
    for (int dd = 1; dd <= 3; ++dd) {
        for (int sign : {-1, 1}) {
            auto cand = m.find(time::to_iso(time::add_days(d, sign * dd)));
            if (cand != m.end()) return cand->second;
        }
    }
    return std::nullopt;
}

std::optional<double> ConsensusProvider::consensus(EventKind k, time::Date d) const {
    const_cast<ConsensusProvider*>(this)->load_kind(k);
    auto r = lookup(k, d);
    return r ? r->consensus : std::nullopt;
}

std::optional<double> ConsensusProvider::previous(EventKind k, time::Date d) const {
    const_cast<ConsensusProvider*>(this)->load_kind(k);
    auto r = lookup(k, d);
    return r ? r->previous : std::nullopt;
}

std::optional<double> ConsensusProvider::actual(EventKind k, time::Date d) const {
    const_cast<ConsensusProvider*>(this)->load_kind(k);
    auto r = lookup(k, d);
    return r ? r->actual : std::nullopt;
}

void ConsensusProvider::enrich(EventKind k, std::vector<HistoricalRelease>& history) const {
    const_cast<ConsensusProvider*>(this)->load_kind(k);
    auto it = tables_.find(static_cast<int>(k));
    if (it == tables_.end() || it->second.empty()) return;
    std::size_t filled = 0;
    for (auto& r : history) {
        auto row = lookup(k, r.date);
        if (!row) continue;
        if (row->consensus) { r.consensus = *row->consensus; ++filled; }
        if (row->previous)  r.previous = *row->previous;
    }
    log::info("Consensus enrichment[" + slug(k) + "]: " +
              std::to_string(filled) + "/" + std::to_string(history.size()));
}

} // namespace macro::data
