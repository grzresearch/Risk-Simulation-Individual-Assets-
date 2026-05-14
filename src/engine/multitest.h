#pragma once

#include <string>
#include <vector>

namespace macro::engine {

struct TestRow {
    std::string label;      // e.g. "spy:CPI:5d"
    double      p_value = 1.0;
    double      bonferroni = 1.0;  // capped at 1.0
    double      bh_q = 1.0;        // Benjamini-Hochberg adjusted q-value
    bool        rejected_at_5pct_fdr = false;
};

// Apply Bonferroni (family-wise) and Benjamini-Hochberg (FDR) corrections.
// Returns rows in the same order as input.
std::vector<TestRow>
apply_corrections(std::vector<TestRow> rows, double fdr_alpha = 0.05);

} // namespace macro::engine
