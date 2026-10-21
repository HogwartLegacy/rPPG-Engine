#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "rppg/dsp.hpp"

namespace rppg::dsp {

CubicSpline::CubicSpline(const Signal& t, const Signal& y) : t_(t), y_(y) {
    const std::size_t n = t.size();
    if (n != y.size()) throw std::invalid_argument("CubicSpline: size mismatch");
    m_.assign(n, 0.0);
    if (n < 3) return;
    // Natural spline: tridiagonal system for second derivatives (Thomas algorithm).
    Signal a(n, 0.0), b(n, 1.0), c(n, 0.0), d(n, 0.0);
    for (std::size_t i = 1; i + 1 < n; ++i) {
        const double h0 = t[i] - t[i - 1];
        const double h1 = t[i + 1] - t[i];
        a[i] = h0;
        b[i] = 2.0 * (h0 + h1);
        c[i] = h1;
        d[i] = 6.0 * ((y[i + 1] - y[i]) / h1 - (y[i] - y[i - 1]) / h0);
    }
    for (std::size_t i = 1; i < n; ++i) {
        const double w = a[i] / b[i - 1];
        b[i] -= w * c[i - 1];
        d[i] -= w * d[i - 1];
    }
    m_[n - 1] = d[n - 1] / b[n - 1];
    for (std::size_t i = n - 1; i-- > 0;) m_[i] = (d[i] - c[i] * m_[i + 1]) / b[i];
}

double CubicSpline::operator()(double x) const {
    const std::size_t n = t_.size();
    if (n == 0) return 0.0;
    if (n == 1) return y_[0];
    if (x <= t_.front()) return y_.front();
    if (x >= t_.back()) return y_.back();
    const std::size_t hi = static_cast<std::size_t>(std::upper_bound(t_.begin(), t_.end(), x) - t_.begin());
    const std::size_t lo = hi - 1;
    const double h = t_[hi] - t_[lo];
    const double A = (t_[hi] - x) / h;
    const double B = (x - t_[lo]) / h;
    if (n < 3) return A * y_[lo] + B * y_[hi];
    return A * y_[lo] + B * y_[hi] + ((A * A * A - A) * m_[lo] + (B * B * B - B) * m_[hi]) * (h * h) / 6.0;
}

Signal resample_uniform(const Signal& t, const Signal& y, double t0, double fs, std::size_t n, Interp method) {
    Signal out(n, 0.0);
    if (t.empty()) return out;
    // Drop non-increasing timestamps (duplicated frames from some camera stacks).
    Signal tt, yy;
    tt.reserve(t.size());
    yy.reserve(t.size());
    for (std::size_t i = 0; i < t.size(); ++i) {
        if (!tt.empty() && t[i] <= tt.back()) continue;
        tt.push_back(t[i]);
        yy.push_back(y[i]);
    }
    if (method == Interp::Cubic && tt.size() >= 3) {
        const CubicSpline sp(tt, yy);
        for (std::size_t k = 0; k < n; ++k) out[k] = sp(t0 + static_cast<double>(k) / fs);
        return out;
    }
    std::size_t j = 0;
    for (std::size_t k = 0; k < n; ++k) {
        const double x = t0 + static_cast<double>(k) / fs;
        if (x <= tt.front()) { out[k] = yy.front(); continue; }
        if (x >= tt.back()) { out[k] = yy.back(); continue; }
        while (j + 1 < tt.size() && tt[j + 1] < x) ++j;
        const double w = (x - tt[j]) / (tt[j + 1] - tt[j]);
        out[k] = yy[j] * (1.0 - w) + yy[j + 1] * w;
    }
    return out;
}

}  // namespace rppg::dsp
