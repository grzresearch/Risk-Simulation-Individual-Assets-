#pragma once

#include "models.h"

#include <optional>
#include <string>
#include <unordered_map>

namespace macro::data {

// Plug-in consensus feed. Default implementation reads from
// `<cache_dir>/consensus/<event_kind>.csv` with rows:
//   yyyy-mm-dd,consensus,actual,previous
// Header optional. Missing fields are ignored gracefully.
class ConsensusProvider {
public:
    explicit ConsensusProvider(std::string base_dir);

    // Returns consensus for an event date if known. Date matching is tolerant
    // (+/- 3 calendar days) to absorb publication-vs-release misalignment.
    std::optional<double> consensus(EventKind k, time::Date date) const;
    std::optional<double> previous(EventKind k, time::Date date) const;
    std::optional<double> actual(EventKind k, time::Date date)    const;

    // Enrich a raw FRED-only history with consensus where possible.
    void enrich(EventKind k, std::vector<HistoricalRelease>& history) const;

    // Total number of (event_kind, date) rows loaded.
    std::size_t row_count() const { return total_rows_; }

private:
    struct Row {
        std::optional<double> consensus;
        std::optional<double> previous;
        std::optional<double> actual;
    };
    using DateMap = std::unordered_map<std::string, Row>;

    std::string base_dir_;
    std::unordered_map<int /*EventKind*/, DateMap> tables_;
    std::size_t total_rows_ = 0;

    void load_kind(EventKind k);
    std::optional<Row> lookup(EventKind k, time::Date d) const;
};

} // namespace macro::data
