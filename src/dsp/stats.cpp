#include <algorithm>
#include <cmath>
#include <numeric>

#include "rppg/dsp.hpp"

namespace rppg::dsp {

double mean(const Signal& x) {
    if (x.empty()) return 0.0;
    return std::accumulate(x.begin(), x.end(), 0.0) / static_cast<double>(x.size());
}

double variance(const Signal& x, int ddof) {
    const std::size_t n = x.size();
    if (n <= static_cast<std::size_t>(ddof)) return 0.0;
    const double m = mean(x);
    double acc = 0.0;
    for (double v : x) acc += (v - m) * (v - m);
    return acc / static_cast<double>(n - static_cast<std::size_t>(ddof));
}

double stdev(const Signal& x, int ddof) { return std::sqrt(variance(x, ddof)); }

double median(Signal x) {
    if (x.empty()) return 0.0;
    const std::size_t n = x.size();
    const std::size_t mid = n / 2;
    std::nth_element(x.begin(), x.begin() + static_cast<std::ptrdiff_t>(mid), x.end());
    const double hi = x[mid];
    if (n % 2 == 1) return hi;
    const double lo = *std::max_element(x.begin(), x.begin() + static_cast<std::ptrdiff_t>(mid));
    return 0.5 * (lo + hi);
}

double mad(const Signal& x) {
    const double m = median(x);
    Signal d(x.size());
    for (std::size_t i = 0; i < x.size(); ++i) d[i] = std::abs(x[i] - m);
    return median(std::move(d));
}

double percentile(Signal x, double p) {
    if (x.empty()) return 0.0;
    std::sort(x.begin(), x.end());
    const double pos = std::clamp(p, 0.0, 100.0) / 100.0 * static_cast<double>(x.size() - 1);
    const std::size_t lo = static_cast<std::size_t>(std::floor(pos));
    const std::size_t hi = std::min(lo + 1, x.size() - 1);
    const double w = pos - static_cast<double>(lo);
    return x[lo] * (1.0 - w) + x[hi] * w;
}

double skewness(const Signal& x) {
    const double m = mean(x);
    const double s = stdev(x);
    if (s <= 0.0) return 0.0;
    double acc = 0.0;
    for (double v : x) acc += std::pow((v - m) / s, 3);
    return acc / static_cast<double>(x.size());
}

double kurtosis(const Signal& x) {
    const double m = mean(x);
    const double s = stdev(x);
    if (s <= 0.0) return 0.0;
    double acc = 0.0;
    for (double v : x) acc += std::pow((v - m) / s, 4);
    return acc / static_cast<double>(x.size()) - 3.0;
}

double pearson(const Signal& a, const Signal& b) {
    const std::size_t n = std::min(a.size(), b.size());
    if (n < 2) return 0.0;
    double ma = 0.0, mb = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        ma += a[i];
        mb += b[i];
    }
    ma /= static_cast<double>(n);
    mb /= static_cast<double>(n);
    double sab = 0.0, saa = 0.0, sbb = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double da = a[i] - ma, db = b[i] - mb;
        sab += da * db;
        saa += da * da;
        sbb += db * db;
    }
    if (saa <= 0.0 || sbb <= 0.0) return 0.0;
    return sab / std::sqrt(saa * sbb);
}

Signal autocorr(const Signal& x, std::size_t max_lag) {
    const std::size_t n = x.size();
    Signal r(max_lag + 1, 0.0);
    if (n == 0) return r;
    const double m = mean(x);
    double r0 = 0.0;
    for (double v : x) r0 += (v - m) * (v - m);
    if (r0 <= 0.0) return r;
    for (std::size_t k = 0; k <= max_lag && k < n; ++k) {
        double acc = 0.0;
        for (std::size_t i = 0; i + k < n; ++i) acc += (x[i] - m) * (x[i + k] - m);
        r[k] = acc / r0;
    }
    return r;
}

void eig_sym3(const Mat3& Ain, Vec3& evals, Mat3& V) {
    Mat3 A = Ain;
    V = {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
    for (int sweep = 0; sweep < 64; ++sweep) {
        const double off = A[0][1] * A[0][1] + A[0][2] * A[0][2] + A[1][2] * A[1][2];
        if (off < 1e-30) break;
        for (int p = 0; p < 2; ++p) {
            for (int q = p + 1; q < 3; ++q) {
                if (std::abs(A[p][q]) < 1e-300) continue;
                const double theta = (A[q][q] - A[p][p]) / (2.0 * A[p][q]);
                const double t = (theta >= 0 ? 1.0 : -1.0) / (std::abs(theta) + std::sqrt(theta * theta + 1.0));
                const double c = 1.0 / std::sqrt(t * t + 1.0);
                const double s = t * c;
                for (int k = 0; k < 3; ++k) {
                    const double akp = A[k][p], akq = A[k][q];
                    A[k][p] = c * akp - s * akq;
                    A[k][q] = s * akp + c * akq;
                }
                for (int k = 0; k < 3; ++k) {
                    const double apk = A[p][k], aqk = A[q][k];
                    A[p][k] = c * apk - s * aqk;
                    A[q][k] = s * apk + c * aqk;
                }
                for (int k = 0; k < 3; ++k) {
                    const double vkp = V[k][p], vkq = V[k][q];
                    V[k][p] = c * vkp - s * vkq;
                    V[k][q] = s * vkp + c * vkq;
                }
            }
        }
    }
    Vec3 d = {A[0][0], A[1][1], A[2][2]};
    std::array<int, 3> idx = {0, 1, 2};
    std::sort(idx.begin(), idx.end(), [&](int a, int b) { return d[a] > d[b]; });
    Mat3 Vs{};
    for (int j = 0; j < 3; ++j) {
        evals[j] = d[idx[j]];
        for (int k = 0; k < 3; ++k) Vs[k][j] = V[k][idx[j]];
    }
    V = Vs;
}

bool solve3(const Mat3& A, const Vec3& b, Vec3& x) {
    const double det = A[0][0] * (A[1][1] * A[2][2] - A[1][2] * A[2][1]) -
                       A[0][1] * (A[1][0] * A[2][2] - A[1][2] * A[2][0]) +
                       A[0][2] * (A[1][0] * A[2][1] - A[1][1] * A[2][0]);
    if (std::abs(det) < 1e-300) return false;
    for (int c = 0; c < 3; ++c) {
        Mat3 M = A;
        for (int r = 0; r < 3; ++r) M[r][c] = b[r];
        const double dc = M[0][0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1]) -
                          M[0][1] * (M[1][0] * M[2][2] - M[1][2] * M[2][0]) +
                          M[0][2] * (M[1][0] * M[2][1] - M[1][1] * M[2][0]);
        x[c] = dc / det;
    }
    return true;
}

}  // namespace rppg::dsp
