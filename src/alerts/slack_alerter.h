#pragma once

#include "engine/scanner.h"

#include <string>
#include <vector>

namespace macro::alerts {

struct AlertRule {
    // Fire when |P(up) - 0.5| > prob_threshold AND BH-q < bh_q_threshold.
    double prob_threshold  = 0.20;  // i.e. abs(p-0.5) > 0.20 means P>0.70 or P<0.30
    double bh_q_threshold  = 0.10;  // FDR-adjusted significance
    int    min_sample_size = 20;
};

struct AlertItem {
    std::string asset_id;
    std::string event_kind;
    std::string event_date;
    std::string regime;
    double prob_up = 0.5;
    double weighted_prob_up = 0.5;
    double bh_q = 1.0;
    int    sample_size = 0;
    double mean_return = 0.0;
};

std::vector<AlertItem> evaluate(const engine::ScanOutput& s, const AlertRule& rule);

// Post a Slack message via an incoming-webhook URL. Returns true on 2xx.
bool send_slack(const std::string& webhook_url,
                const std::vector<AlertItem>& alerts,
                const std::string& heading = "macro_risk alert");

} // namespace macro::alerts
