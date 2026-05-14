#pragma once

#include "models.h"

#include <vector>

namespace macro::engine {

// Build a DirectionalForecast from a sample of event-conditional log returns.
DirectionalForecast forecast_from_sample(const std::vector<double>& sample);

// Combine event reactions across the requested horizon. Picks the closest
// supported horizon (1, 5, or 20) from each EventReaction.
std::vector<double>
returns_at_horizon(const std::vector<EventReaction>& reactions, int horizon);

// Compute a slope (sensitivity) of return to release "surprise" if surprises
// are available (non-zero variance), otherwise returns nullopt.
std::optional<double>
surprise_sensitivity(const std::vector<EventReaction>& reactions, int horizon);

} // namespace macro::engine
