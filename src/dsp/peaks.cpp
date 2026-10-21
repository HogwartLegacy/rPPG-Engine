#include <algorithm>
#include <cmath>

#include "rppg/dsp.hpp"

namespace rppg::dsp {

double peak_prominence(const Signal& x, std::size_t i) {
    const std::size_t n = x.size();
    if (i >= n) return 0.0;
    const double h = x[i];
    double left_min = h;
    for (std::size_t j = i; j-- > 0;) {
        if (x[j] > h) break;
        left_min = std::min(left_min, x[j]);
    }
    double right_min = h;
    for (std::size_t j = i + 1; j < n; ++j) {
        if (x[j] > h) break;
        right_min = std::min(right_min, x[j]);
    }
    return h - std::max(left_min, right_min);
}

std::vector<std::size_t> find_peaks(const Signal& x, std::size_t min_distance, double min_prominence) {
    const std::size_t n = x.size();
    std::vector<std::size_t> cand;
    if (n < 3) return cand;
    // Local maxima, plateaus reported at their middle sample.
    std::size_t i = 1;
    while (i + 1 < n) {
        if (x[i - 1] < x[i]) {
            std::size_t ahead = i + 1;
            while (ahead + 1 < n && x[ahead] == x[i]) ++ahead;
            if (x[ahead] < x[i]) {
                cand.push_back((i + ahead - 1) / 2);
                i = ahead;
                continue;
            }
        }
        ++i;
    }
    if (min_distance > 1 && cand.size() > 1) {
        std::vector<std::size_t> order(cand.size());
        for (std::size_t k = 0; k < order.size(); ++k) order[k] = k;
        std::stable_sort(order.begin(), order.end(),
                         [&](std::size_t a, std::size_t b) { return x[cand[a]] > x[cand[b]]; });
        std::vector<char> keep(cand.size(), 1);
        for (std::size_t oi = 0; oi < order.size(); ++oi) {
            const std::size_t k = order[oi];
            if (!keep[k]) continue;
            for (std::size_t j = k; j-- > 0;) {
                if (cand[k] - cand[j] >= min_distance) break;
                keep[j] = 0;
            }
            for (std::size_t j = k + 1; j < cand.size(); ++j) {
                if (cand[j] - cand[k] >= min_distance) break;
                keep[j] = 0;
            }
        }
        std::vector<std::size_t> kept;
        for (std::size_t k = 0; k < cand.size(); ++k)
            if (keep[k]) kept.push_back(cand[k]);
        cand.swap(kept);
    }
    if (min_prominence > 0.0) {
        std::vector<std::size_t> kept;
        for (std::size_t p : cand)
            if (peak_prominence(x, p) >= min_prominence) kept.push_back(p);
        cand.swap(kept);
    }
    return cand;
}

double refine_peak(const Signal& x, std::size_t i) {
    if (i == 0 || i + 1 >= x.size()) return static_cast<double>(i);
    const double a = x[i - 1], b = x[i], c = x[i + 1];
    const double den = a - 2.0 * b + c;
    if (std::abs(den) < 1e-300) return static_cast<double>(i);
    const double d = std::clamp(0.5 * (a - c) / den, -0.5, 0.5);
    return static_cast<double>(i) + d;
}

}  // namespace rppg::dsp
