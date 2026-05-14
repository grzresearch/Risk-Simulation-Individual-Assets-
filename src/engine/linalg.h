#pragma once

#include <vector>

namespace macro::engine::linalg {

using Matrix = std::vector<std::vector<double>>;

// Cholesky factorisation: A = L * L^T (lower triangular).
// Uses jittering on the diagonal if A is non-PD (returns approximate fix).
Matrix cholesky(const Matrix& A);

// Multiply L * z (vector). Assumes L is lower triangular.
std::vector<double> lt_mul(const Matrix& L, const std::vector<double>& z);

// Empirical correlation matrix from `cols` (each column == a series of equal length).
Matrix correlation(const std::vector<std::vector<double>>& cols);

// Empirical covariance matrix.
Matrix covariance(const std::vector<std::vector<double>>& cols);

} // namespace macro::engine::linalg
