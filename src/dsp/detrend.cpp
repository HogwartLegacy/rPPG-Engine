#include <cmath>

#include "rppg/dsp.hpp"

namespace rppg::dsp {

Signal whittaker_smooth(const Signal& x, double mu) {
    const std::size_t n = x.size();
    if (n < 4 || mu <= 0.0) return x;
    // A = I + mu * D2'D2 is symmetric positive definite and pentadiagonal.
    Signal a0(n), a1(n - 1), a2(n - 2);
    for (std::size_t i = 0; i < n; ++i) {
        double d;
        if (i == 0 || i == n - 1)
            d = 1.0;
        else if (i == 1 || i == n - 2)
            d = 5.0;
        else
            d = 6.0;
        a0[i] = 1.0 + mu * d;
    }
    for (std::size_t i = 0; i + 1 < n; ++i) a1[i] = mu * ((i == 0 || i == n - 2) ? -2.0 : -4.0);
    for (std::size_t i = 0; i + 2 < n; ++i) a2[i] = mu;

    // Banded LDL^T.
    Signal D(n), l1(n, 0.0), l2(n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        double d = a0[i];
        if (i >= 1) d -= l1[i - 1] * l1[i - 1] * D[i - 1];
        if (i >= 2) d -= l2[i - 2] * l2[i - 2] * D[i - 2];
        D[i] = d;
        if (i + 1 < n) {
            double v = a1[i];
            if (i >= 1) v -= l2[i - 1] * l1[i - 1] * D[i - 1];
            l1[i] = v / d;
        }
        if (i + 2 < n) l2[i] = a2[i] / d;
    }
    Signal z(n);
    for (std::size_t i = 0; i < n; ++i) {
        double v = x[i];
        if (i >= 1) v -= l1[i - 1] * z[i - 1];
        if (i >= 2) v -= l2[i - 2] * z[i - 2];
        z[i] = v;
    }
    for (std::size_t i = 0; i < n; ++i) z[i] /= D[i];
    for (std::size_t ii = n; ii-- > 0;) {
        double v = z[ii];
        if (ii + 1 < n) v -= l1[ii] * z[ii + 1];
        if (ii + 2 < n) v -= l2[ii] * z[ii + 2];
        z[ii] = v;
    }
    return z;
}

Signal detrend_tarvainen(const Signal& x, double lambda) {
    const Signal trend = whittaker_smooth(x, lambda * lambda);
    Signal y(x.size());
    for (std::size_t i = 0; i < x.size(); ++i) y[i] = x[i] - trend[i];
    return y;
}

Signal smoothing_spline(const Signal& x, double lambda_s3, double fs) {
    // sum (h_k - x_k)^2 + lambda * int h''^2 dt  ~  sum (h_k - x_k)^2 + lambda fs^3 sum (D2 h)^2
    return whittaker_smooth(x, lambda_s3 * fs * fs * fs);
}

Signal remove_mean(const Signal& x) {
    const double m = mean(x);
    Signal y(x.size());
    for (std::size_t i = 0; i < x.size(); ++i) y[i] = x[i] - m;
    return y;
}

Signal zscore(const Signal& x) {
    const double m = mean(x);
    const double s = stdev(x);
    Signal y(x.size());
    const double inv = s > 1e-12 ? 1.0 / s : 0.0;
    for (std::size_t i = 0; i < x.size(); ++i) y[i] = (x[i] - m) * inv;
    return y;
}

Signal moving_average(const Signal& x, std::size_t width) {
    const std::size_t n = x.size();
    if (width <= 1 || n == 0) return x;
    Signal c(n + 1, 0.0);
    for (std::size_t i = 0; i < n; ++i) c[i + 1] = c[i] + x[i];
    const std::size_t h = width / 2;
    Signal y(n);
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t lo = i >= h ? i - h : 0;
        const std::size_t hi = std::min(n, i + (width - h));
        y[i] = (c[hi] - c[lo]) / static_cast<double>(hi - lo);
    }
    return y;
}

}  // namespace rppg::dsp
