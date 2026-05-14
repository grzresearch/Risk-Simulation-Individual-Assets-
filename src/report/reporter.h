#pragma once

#include "engine/scanner.h"
#include "models.h"

#include <iosfwd>
#include <string>
#include <vector>

namespace macro::report {

void print_text(std::ostream& out, const engine::ScanOutput& result);
std::string to_json(const engine::ScanOutput& result, bool pretty = true);

} // namespace macro::report
