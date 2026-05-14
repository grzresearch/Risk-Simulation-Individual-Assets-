#include "report/csv_reporter.h"

#include "util/time_utils.h"

#include <iomanip>
#include <ostream>
#include <sstream>

namespace macro::report {

namespace {
std::string esc(std::string s) {
    bool quote = s.find(',') != std::string::npos || s.find('"') != std::string::npos;
    for (std::size_t i = 0; i < s.size(); ++i) if (s[i] == '"') { s.insert(i, "\""); ++i; }
    return quote ? "\"" + s + "\"" : s;
}
} // namespace

void to_csv(std::ostream& out, const engine::ScanOutput& result) {
    out << "asset_id,asset_name,class,event_date,event_kind,regime,"
           "prob_up,weighted_prob_up,regime_prob_up,mean_return,median_return,"
           "ci95_low,ci95_high,var95,es95,sample_size,sign_test_p,bonferroni_p,bh_q,"
           "surprise_beta,expected_surprise_z,garch_sigma_1d,"
           "mc_p05,mc_p50,mc_p95,spot,hist_vol,garch_vol\n";

    for (const auto& r : result.assets) {
        for (const auto& s : r.scenarios) {
            const auto& f = s.forecast;
            out << esc(r.asset.id) << ','
                << esc(r.asset.display_name) << ','
                << to_string(r.asset.asset_class) << ','
                << time::to_iso(s.event.date) << ','
                << esc(to_string(s.event.kind)) << ','
                << esc(s.regime) << ','
                << f.prob_up << ','
                << f.weighted_prob_up << ','
                << f.regime_prob_up << ','
                << f.mean_return << ','
                << f.median_return << ','
                << f.ci95_low << ','
                << f.ci95_high << ','
                << f.var95 << ','
                << f.es95 << ','
                << f.sample_size << ','
                << f.sign_test_p << ','
                << f.bonferroni_p << ','
                << f.bh_q << ','
                << (s.surprise_beta ? std::to_string(*s.surprise_beta) : "") << ','
                << (s.expected_surprise_z ? std::to_string(*s.expected_surprise_z) : "") << ','
                << s.garch_sigma_1d << ','
                << s.mc_price_p05 << ','
                << s.mc_price_p50 << ','
                << s.mc_price_p95 << ','
                << r.spot_price << ','
                << r.hist_vol_annualized << ','
                << r.garch_vol_annualized << '\n';
        }
    }
}

} // namespace macro::report
