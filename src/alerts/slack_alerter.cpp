#include "alerts/slack_alerter.h"

#include "util/http_client.h"
#include "util/logger.h"
#include "util/time_utils.h"

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <cmath>
#include <sstream>

namespace macro::alerts {

std::vector<AlertItem> evaluate(const engine::ScanOutput& s, const AlertRule& rule) {
    std::vector<AlertItem> out;
    for (const auto& r : s.assets) {
        for (const auto& sc : r.scenarios) {
            const auto& f = sc.forecast;
            if (f.sample_size < rule.min_sample_size) continue;
            if (f.bh_q >= rule.bh_q_threshold)        continue;
            if (std::fabs(f.prob_up - 0.5) <= rule.prob_threshold) continue;
            AlertItem a;
            a.asset_id   = r.asset.id;
            a.event_kind = to_string(sc.event.kind);
            a.event_date = time::to_iso(sc.event.date);
            a.regime     = sc.regime;
            a.prob_up    = f.prob_up;
            a.weighted_prob_up = f.weighted_prob_up;
            a.bh_q       = f.bh_q;
            a.sample_size = f.sample_size;
            a.mean_return = f.mean_return;
            out.push_back(a);
        }
    }
    return out;
}

namespace {

size_t discard_cb(char*, size_t size, size_t n, void*) { return size * n; }

bool curl_post_json(const std::string& url, const std::string& body) {
    CURL* c = curl_easy_init();
    if (!c) return false;
    curl_slist* hdrs = curl_slist_append(nullptr, "Content-Type: application/json");
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdrs);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, discard_cb);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 10L);
    CURLcode rc = curl_easy_perform(c);
    long status = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(hdrs);
    curl_easy_cleanup(c);
    return rc == CURLE_OK && status >= 200 && status < 300;
}

} // namespace

bool send_slack(const std::string& webhook_url,
                const std::vector<AlertItem>& alerts,
                const std::string& heading) {
    if (webhook_url.empty() || alerts.empty()) return false;

    std::ostringstream text;
    text << "*" << heading << "*\n";
    for (const auto& a : alerts) {
        text << "> `" << a.asset_id << "`  *" << a.event_kind << "*  on "
             << a.event_date << "  (" << a.regime << ")\n"
             << "  P(up)=" << static_cast<int>(a.prob_up * 100) << "%,  "
             << "EWMA=" << static_cast<int>(a.weighted_prob_up * 100) << "%,  "
             << "mean=" << (a.mean_return >= 0 ? "+" : "")
             << static_cast<int>(a.mean_return * 10000) / 100.0 << "%,  "
             << "BH-q=" << a.bh_q << ",  n=" << a.sample_size << "\n";
    }
    nlohmann::json body = {{"text", text.str()}};
    bool ok = curl_post_json(webhook_url, body.dump());
    if (!ok) log::warn("slack alert failed");
    return ok;
}

} // namespace macro::alerts
