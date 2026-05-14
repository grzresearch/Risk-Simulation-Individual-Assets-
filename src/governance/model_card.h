#pragma once

#include "config.h"
#include "engine/scanner.h"

#include <iosfwd>

namespace macro::governance {

// Generates a Markdown "model card" capturing the engine's purpose, the
// inputs that produced a scan, the modelling assumptions, validation
// evidence (back-test metrics) and known limitations. Aligned with the
// SR 11-7 model risk management spirit.
void write_model_card(std::ostream& out,
                      const Config& cfg,
                      const engine::ScanOutput& sample_output);

} // namespace macro::governance
