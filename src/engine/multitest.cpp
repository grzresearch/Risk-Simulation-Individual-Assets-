#include "engine/multitest.h"

#include <algorithm>
#include <numeric>

namespace macro::engine {

std::vector<TestRow>
apply_corrections(std::vector<TestRow> rows, double fdr_alpha) {
    const std::size_t m = rows.size();
    if (m == 0) return rows;

    // Bonferroni
    for (auto& r : rows) {
        r.bonferroni = std::min(1.0, r.p_value * static_cast<double>(m));
    }

    // Benjamini-Hochberg: sort by p, compute q_i = min over k>=i (p_(k) * m / k)
    std::vector<std::size_t> idx(m);
    std::iota(idx.begin(), idx.end(), 0u);
    std::sort(idx.begin(), idx.end(),
              [&](std::size_t a, std::size_t b) { return rows[a].p_value < rows[b].p_value; });

    std::vector<double> q(m, 1.0);
    double running_min = 1.0;
    for (std::size_t r = m; r-- > 0; ) {
        std::size_t i = idx[r];
        double rank  = static_cast<double>(r + 1);
        double cand  = rows[i].p_value * static_cast<double>(m) / rank;
        running_min  = std::min(running_min, cand);
        q[i] = std::min(1.0, running_min);
    }
    for (std::size_t i = 0; i < m; ++i) {
        rows[i].bh_q = q[i];
        rows[i].rejected_at_5pct_fdr = (rows[i].bh_q < fdr_alpha);
    }
    return rows;
}

} // namespace macro::engine
