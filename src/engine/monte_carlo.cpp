#include "engine/monte_carlo.h"

#include <cmath>
#include <random>

#ifdef _OPENMP
#  include <omp.h>
#endif

namespace macro::engine {

std::vector<double>
gbm_terminal_prices(double spot, double mu, double sigma,
                    int horizon_days, int paths, unsigned seed) {
    std::vector<double> out(static_cast<std::size_t>(std::max(0, paths)));
#pragma omp parallel
    {
#ifdef _OPENMP
        unsigned tseed = seed ^ static_cast<unsigned>(omp_get_thread_num() * 2654435761u);
#else
        unsigned tseed = seed;
#endif
        std::mt19937 rng(tseed);
        std::normal_distribution<double> n(0.0, 1.0);
#pragma omp for schedule(static)
        for (int i = 0; i < paths; ++i) {
            double s = spot;
            for (int t = 0; t < horizon_days; ++t) {
                double z = n(rng);
                s *= std::exp((mu - 0.5 * sigma * sigma) + sigma * z);
            }
            out[static_cast<std::size_t>(i)] = s;
        }
    }
    return out;
}

std::vector<double>
mixture_terminal_prices(double spot,
                        const std::vector<double>& event_returns,
                        const std::vector<double>& baseline_daily_returns,
                        int horizon_days,
                        int paths,
                        double weight_event,
                        unsigned seed) {
    if (paths <= 0 || horizon_days <= 0) return {};
    std::vector<double> out(static_cast<std::size_t>(paths));
    const std::size_t ne = event_returns.size();
    const std::size_t nb = baseline_daily_returns.size();

#pragma omp parallel
    {
#ifdef _OPENMP
        unsigned tseed = seed ^ static_cast<unsigned>(omp_get_thread_num() * 2654435761u);
#else
        unsigned tseed = seed;
#endif
        std::mt19937 rng(tseed);
        std::uniform_real_distribution<double> u(0.0, 1.0);
#pragma omp for schedule(static)
        for (int i = 0; i < paths; ++i) {
            double s = spot;
            bool used_event = false;
            if (ne > 0 && u(rng) < weight_event) {
                std::uniform_int_distribution<std::size_t> de(0, ne - 1);
                s *= std::exp(event_returns[de(rng)]);
                used_event = true;
            }
            if (nb > 0) {
                std::uniform_int_distribution<std::size_t> db(0, nb - 1);
                int t0 = used_event ? 1 : 0;
                for (int t = t0; t < horizon_days; ++t) {
                    s *= std::exp(baseline_daily_returns[db(rng)]);
                }
            }
            out[static_cast<std::size_t>(i)] = s;
        }
    }
    return out;
}

} // namespace macro::engine

