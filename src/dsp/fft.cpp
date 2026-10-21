#include <cmath>

#include "rppg/dsp.hpp"

namespace rppg::dsp {

std::size_t next_pow2(std::size_t n) {
    std::size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}

namespace {

void fft_radix2(std::vector<cpx>& a, bool inverse) {
    const std::size_t n = a.size();
    if (n <= 1) return;
    for (std::size_t i = 1, j = 0; i < n; ++i) {
        std::size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (std::size_t len = 2; len <= n; len <<= 1) {
        const double ang = 2.0 * M_PI / static_cast<double>(len) * (inverse ? 1.0 : -1.0);
        const cpx wlen(std::cos(ang), std::sin(ang));
        const std::size_t half = len >> 1;
        for (std::size_t i = 0; i < n; i += len) {
            cpx w(1.0, 0.0);
            for (std::size_t k = 0; k < half; ++k) {
                const cpx u = a[i + k];
                const cpx v = a[i + k + half] * w;
                a[i + k] = u + v;
                a[i + k + half] = u - v;
                // Re-synchronise the twiddle periodically to limit drift on long transforms.
                if ((k & 63) == 63) {
                    const double t = ang * static_cast<double>(k + 1);
                    w = cpx(std::cos(t), std::sin(t));
                } else {
                    w *= wlen;
                }
            }
        }
    }
    if (inverse) {
        const double s = 1.0 / static_cast<double>(n);
        for (auto& v : a) v *= s;
    }
}

// Bluestein chirp-z for arbitrary n.
void fft_bluestein(std::vector<cpx>& a, bool inverse) {
    const std::size_t n = a.size();
    const std::size_t m = next_pow2(2 * n - 1);
    const double sgn = inverse ? 1.0 : -1.0;
    std::vector<cpx> w(n);
    for (std::size_t k = 0; k < n; ++k) {
        // k^2 mod 2n keeps the angle argument small and exact.
        const unsigned long long k2 = (static_cast<unsigned long long>(k) * k) % (2ULL * n);
        const double ang = sgn * M_PI * static_cast<double>(k2) / static_cast<double>(n);
        w[k] = cpx(std::cos(ang), std::sin(ang));
    }
    std::vector<cpx> A(m, cpx(0.0, 0.0)), B(m, cpx(0.0, 0.0));
    for (std::size_t k = 0; k < n; ++k) A[k] = a[k] * w[k];
    B[0] = std::conj(w[0]);
    for (std::size_t k = 1; k < n; ++k) B[k] = B[m - k] = std::conj(w[k]);
    fft_radix2(A, false);
    fft_radix2(B, false);
    for (std::size_t k = 0; k < m; ++k) A[k] *= B[k];
    fft_radix2(A, true);
    for (std::size_t k = 0; k < n; ++k) a[k] = A[k] * w[k];
    if (inverse) {
        const double s = 1.0 / static_cast<double>(n);
        for (auto& v : a) v *= s;
    }
}

}  // namespace

void fft(std::vector<cpx>& a, bool inverse) {
    const std::size_t n = a.size();
    if (n <= 1) return;
    if ((n & (n - 1)) == 0)
        fft_radix2(a, inverse);
    else
        fft_bluestein(a, inverse);
}

Signal power_spectrum(const Signal& x, std::size_t nfft) {
    std::vector<cpx> buf(nfft, cpx(0.0, 0.0));
    const std::size_t n = std::min(nfft, x.size());
    for (std::size_t i = 0; i < n; ++i) buf[i] = cpx(x[i], 0.0);
    fft(buf, false);
    Signal p(nfft / 2 + 1);
    for (std::size_t k = 0; k < p.size(); ++k) p[k] = std::norm(buf[k]);
    return p;
}

}  // namespace rppg::dsp
