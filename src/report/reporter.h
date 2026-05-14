#pragma once

#include "models.h"

#include <iosfwd>
#include <string>
#include <vector>

namespace macro::report {

void print_text(std::ostream& out, const std::vector<AssetReport>& reports);
std::string to_json(const std::vector<AssetReport>& reports, bool pretty = true);

} // namespace macro::report
