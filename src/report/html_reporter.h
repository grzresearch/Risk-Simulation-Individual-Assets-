#pragma once

#include "engine/scanner.h"

#include <iosfwd>

namespace macro::report {

// Standalone HTML page with embedded CSS. No external assets.
void to_html(std::ostream& out, const engine::ScanOutput& result);

} // namespace macro::report
