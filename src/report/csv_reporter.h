#pragma once

#include "engine/scanner.h"

#include <iosfwd>

namespace macro::report {

// Long-format CSV with one row per (asset, event). Suitable for spreadsheet
// pivots and downstream BI tools.
void to_csv(std::ostream& out, const engine::ScanOutput& result);

} // namespace macro::report
