#include "engine/linalg.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace macro::engine::linalg {

Matrix cholesky(const Matrix& A) {
    const std::size_t n = A.size();
    Matrix L(n, std::vector<double>(n, 0.0));
    const double kJitter = 1e-10;
    for (int attempt = 0; attempt < 6; ++attempt) {
        bool ok = true;
        double jitter = kJitter * std::pow(10.0, attempt);
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = 0; j <= i; ++j) {
                double s = A[i][j];
                if (i == j) s += jitter;
                for (std::size_t k = 0; k < j; ++k) s -= L[i][k] * L[j][k];
                if (i == j) {
                    if (s <= 0.0) { ok = false; break; }
                    L[i][j] = std::sqrt(s);
                } else {
                    L[i][j] = s / L[j][j];
                }
            }
            if (!ok) break;
        }
        if (ok) return L;
    }
    // Last resort: identity scaled by sample stdev approximation
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < n; ++j) L[i][j] = (i == j) ? std::sqrt(std::max(1e-12, A[i][i])) : 0.0;
    return L;
}

std::vector<double> lt_mul(const Matrix& L, const std::vector<double>& z) {
    const std::size_t n = L.size();
    std::vector<double> out(n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        double s = 0.0;
        for (std::size_t j = 0; j <= i; ++j) s += L[i][j] * z[j];
        out[i] = s;
    }
    return out;
}

Matrix covariance(const std::vector<std::vector<double>>& cols) {
    const std::size_t k = cols.size();
    Matrix C(k, std::vector<double>(k, 0.0));
    if (k == 0) return C;
    std::size_t n = cols[0].size();
    for (const auto& c : cols) n = std::min(n, c.size());
    if (n < 2) return C;
    std::vector<double> mean(k, 0.0);
    for (std::size_t i = 0; i < k; ++i) {
        for (std::size_t t = 0; t < n; ++t) mean[i] += cols[i][t];
        mean[i] /= static_cast<double>(n);
    }
    for (std::size_t i = 0; i < k; ++i) {
        for (std::size_t j = i; j < k; ++j) {
            double s = 0.0;
            for (std::size_t t = 0; t < n; ++t) {
                s += (cols[i][t] - mean[i]) * (cols[j][t] - mean[j]);
            }
            double c = s / static_cast<double>(n - 1);
            C[i][j] = c;
            C[j][i] = c;
        }
    }
    return C;
}

Matrix correlation(const std::vector<std::vector<double>>& cols) {
    auto cov = covariance(cols);
    const std::size_t k = cov.size();
    Matrix R(k, std::vector<double>(k, 0.0));
    for (std::size_t i = 0; i < k; ++i) {
        for (std::size_t j = 0; j < k; ++j) {
            double denom = std::sqrt(cov[i][i] * cov[j][j]);
            R[i][j] = (denom > 0) ? cov[i][j] / denom : (i == j ? 1.0 : 0.0);
        }
    }
    return R;
}

} // namespace macro::engine::linalg
