#pragma once

#include "models.h"

#include <vector>

namespace macro::engine {

// Rolling z-score of (actual - consensus) over the previous N releases.
// Releases without consensus are skipped. Returns a parallel vector of
// standardised surprises (NaN where undefined).
std::vector<double>
standardised_surprise(const std::vector<HistoricalRelease>& releases,
                      int window = 24);

// Forward standardise a single surprise using the rolling stdev of the
// last `window` historical surprises strictly before `as_of`.
double standardise_one(const std::vector<HistoricalRelease>& history,
                       time::Date as_of,
                       double raw_surprise,
                       int window = 24);

} // namespace macro::engine
