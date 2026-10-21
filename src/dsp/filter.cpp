#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "rppg/dsp.hpp"

namespace rppg::dsp {

namespace {

struct Zpk {
    std::vector<cpx> z, p;
    double k = 1.0;
};

// Analog Butterworth prototype (scipy.signal.buttap).
Zpk buttap(int n) {
    Zpk r;
    for (int m = -n + 1; m < n; m += 2) {
        const double ang = M_PI * m / (2.0 * n);
        r.p.push_back(-cpx(std::cos(ang), std::sin(ang)));
    }
    r.k = 1.0;
    return r;
}

cpx prod(const std::vector<cpx>& v, const cpx& offset, bool negate) {
    cpx acc(1.0, 0.0);
    for (const auto& x : v) acc *= negate ? (offset - x) : (x - offset);
    return acc;
}

Zpk lp2lp(const Zpk& in, double wo) {
    Zpk r = in;
    for (auto& p : r.p) p *= wo;
    for (auto& z : r.z) z *= wo;
    const int degree = static_cast<int>(in.p.size() - in.z.size());
    r.k = in.k * std::pow(wo, degree);
    return r;
}

Zpk lp2hp(const Zpk& in, double wo) {
    Zpk r;
    for (const auto& z : in.z) r.z.push_back(wo / z);
    for (const auto& p : in.p) r.p.push_back(wo / p);
    const std::size_t degree = in.p.size() - in.z.size();
    for (std::size_t i = 0; i < degree; ++i) r.z.push_back(cpx(0.0, 0.0));
    const cpx num = prod(in.z, cpx(0.0, 0.0), true);
    const cpx den = prod(in.p, cpx(0.0, 0.0), true);
    r.k = in.k * (num / den).real();
    return r;
}

Zpk lp2bp(const Zpk& in, double wo, double bw) {
    Zpk r;
    const std::size_t degree = in.p.size() - in.z.size();
    for (const auto& z0 : in.z) {
        const cpx zl = z0 * (bw / 2.0);
        const cpx d = std::sqrt(zl * zl - wo * wo);
        r.z.push_back(zl + d);
        r.z.push_back(zl - d);
    }
    for (const auto& p0 : in.p) {
        const cpx pl = p0 * (bw / 2.0);
        const cpx d = std::sqrt(pl * pl - wo * wo);
        r.p.push_back(pl + d);
        r.p.push_back(pl - d);
    }
    for (std::size_t i = 0; i < degree; ++i) r.z.push_back(cpx(0.0, 0.0));
    r.k = in.k * std::pow(bw, static_cast<double>(degree));
    return r;
}

Zpk bilinear(const Zpk& in, double fs) {
    const double fs2 = 2.0 * fs;
    Zpk r;
    for (const auto& z : in.z) r.z.push_back((fs2 + z) / (fs2 - z));
    for (const auto& p : in.p) r.p.push_back((fs2 + p) / (fs2 - p));
    const std::size_t degree = in.p.size() - in.z.size();
    for (std::size_t i = 0; i < degree; ++i) r.z.push_back(cpx(-1.0, 0.0));
    const cpx num = prod(in.z, cpx(fs2, 0.0), true);
    const cpx den = prod(in.p, cpx(fs2, 0.0), true);
    r.k = in.k * (num / den).real();
    return r;
}

// Pairs conjugate poles into biquads; Butterworth zeros are all real (+1 / -1).
Sos zpk2sos(const Zpk& f) {
    std::vector<cpx> cplx;
    std::vector<double> real_p;
    for (const auto& p : f.p) {
        if (std::abs(p.imag()) > 1e-12) {
            if (p.imag() > 0) cplx.push_back(p);
        } else {
            real_p.push_back(p.real());
        }
    }
    std::vector<double> zeros;
    for (const auto& z : f.z) zeros.push_back(z.real());
    // Interleave +1 and -1 zeros so every band-pass biquad gets (1 - z^-2).
    std::vector<double> pos, neg;
    for (double z : zeros) (z > 0 ? pos : neg).push_back(z);
    std::vector<double> zq;
    while (!pos.empty() || !neg.empty()) {
        if (!pos.empty()) { zq.push_back(pos.back()); pos.pop_back(); }
        if (!neg.empty()) { zq.push_back(neg.back()); neg.pop_back(); }
    }
    std::sort(real_p.begin(), real_p.end());

    Sos sos;
    std::size_t zi = 0;
    auto take_zero = [&](double& z, bool& has) {
        if (zi < zq.size()) { z = zq[zi++]; has = true; } else { z = 0.0; has = false; }
    };
    for (const auto& p : cplx) {
        Biquad q;
        q.a1 = -2.0 * p.real();
        q.a2 = std::norm(p);
        double z1, z2;
        bool h1, h2;
        take_zero(z1, h1);
        take_zero(z2, h2);
        // (1 - z1 x)(1 - z2 x) with x = z^-1
        q.b0 = 1.0;
        q.b1 = -((h1 ? z1 : 0.0) + (h2 ? z2 : 0.0));
        q.b2 = (h1 && h2) ? z1 * z2 : 0.0;
        sos.push_back(q);
    }
    for (std::size_t i = 0; i < real_p.size(); i += 2) {
        Biquad q;
        if (i + 1 < real_p.size()) {
            q.a1 = -(real_p[i] + real_p[i + 1]);
            q.a2 = real_p[i] * real_p[i + 1];
            double z1, z2;
            bool h1, h2;
            take_zero(z1, h1);
            take_zero(z2, h2);
            q.b0 = 1.0;
            q.b1 = -((h1 ? z1 : 0.0) + (h2 ? z2 : 0.0));
            q.b2 = (h1 && h2) ? z1 * z2 : 0.0;
        } else {
            q.a1 = -real_p[i];
            q.a2 = 0.0;
            double z1;
            bool h1;
            take_zero(z1, h1);
            q.b0 = 1.0;
            q.b1 = h1 ? -z1 : 0.0;
            q.b2 = 0.0;
        }
        sos.push_back(q);
    }
    if (!sos.empty()) {
        sos[0].b0 *= f.k;
        sos[0].b1 *= f.k;
        sos[0].b2 *= f.k;
    }
    return sos;
}

// Steady-state initial conditions for a unit step (scipy.signal.sosfilt_zi).
std::vector<std::array<double, 2>> sosfilt_zi(const Sos& sos) {
    std::vector<std::array<double, 2>> zi(sos.size());
    double scale = 1.0;
    for (std::size_t s = 0; s < sos.size(); ++s) {
        const Biquad& q = sos[s];
        const double B0 = q.b1 - q.a1 * q.b0;
        const double B1 = q.b2 - q.a2 * q.b0;
        const double den = 1.0 + q.a1 + q.a2;
        const double z0 = std::abs(den) > 1e-300 ? (B0 + B1) / den : 0.0;
        const double z1 = B1 - q.a2 * z0;
        zi[s] = {scale * z0, scale * z1};
        const double asum = 1.0 + q.a1 + q.a2;
        scale *= std::abs(asum) > 1e-300 ? (q.b0 + q.b1 + q.b2) / asum : 0.0;
    }
    return zi;
}

void run_sos(const Sos& sos, std::vector<std::array<double, 2>>& z, double* x, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
        double v = x[i];
        for (std::size_t s = 0; s < sos.size(); ++s) {
            const Biquad& q = sos[s];
            const double y = q.b0 * v + z[s][0];
            z[s][0] = q.b1 * v - q.a1 * y + z[s][1];
            z[s][1] = q.b2 * v - q.a2 * y;
            v = y;
        }
        x[i] = v;
    }
}

}  // namespace

Sos butter(int order, BandType type, double f1_hz, double f2_hz, double fs) {
    if (order < 1) throw std::invalid_argument("butter: order must be >= 1");
    const double nyq = fs / 2.0;
    const double fsd = 2.0;  // scipy designs with fs = 2 after normalising to Nyquist
    auto warp = [&](double f) {
        const double wn = std::clamp(f / nyq, 1e-9, 1.0 - 1e-9);
        return 2.0 * fsd * std::tan(M_PI * wn / fsd);
    };
    Zpk proto = buttap(order);
    Zpk analog;
    switch (type) {
        case BandType::LowPass: analog = lp2lp(proto, warp(f1_hz)); break;
        case BandType::HighPass: analog = lp2hp(proto, warp(f1_hz)); break;
        case BandType::BandPass: {
            const double wl = warp(f1_hz), wh = warp(f2_hz);
            if (!(wh > wl)) throw std::invalid_argument("butter: f2 must exceed f1");
            analog = lp2bp(proto, std::sqrt(wl * wh), wh - wl);
            break;
        }
    }
    return zpk2sos(bilinear(analog, fsd));
}

Signal sosfilt(const Sos& sos, const Signal& x) {
    Signal y = x;
    std::vector<std::array<double, 2>> z(sos.size(), {0.0, 0.0});
    run_sos(sos, z, y.data(), y.size());
    return y;
}

Signal sosfiltfilt(const Sos& sos, const Signal& x) {
    const std::size_t n = x.size();
    if (n == 0 || sos.empty()) return x;
    std::size_t zero_b2 = 0, zero_a2 = 0;
    for (const auto& q : sos) {
        zero_b2 += (q.b2 == 0.0);
        zero_a2 += (q.a2 == 0.0);
    }
    std::size_t padlen = 3 * (2 * sos.size() + 1 - std::min(zero_b2, zero_a2));
    if (padlen >= n) padlen = n - 1;
    // Odd extension at both ends.
    Signal ext(n + 2 * padlen);
    for (std::size_t i = 0; i < padlen; ++i) ext[i] = 2.0 * x[0] - x[padlen - i];
    for (std::size_t i = 0; i < n; ++i) ext[padlen + i] = x[i];
    for (std::size_t i = 0; i < padlen; ++i) ext[padlen + n + i] = 2.0 * x[n - 1] - x[n - 2 - i];
    const auto zi = sosfilt_zi(sos);
    auto z = zi;
    for (std::size_t s = 0; s < z.size(); ++s) z[s] = {zi[s][0] * ext.front(), zi[s][1] * ext.front()};
    run_sos(sos, z, ext.data(), ext.size());
    std::reverse(ext.begin(), ext.end());
    for (std::size_t s = 0; s < z.size(); ++s) z[s] = {zi[s][0] * ext.front(), zi[s][1] * ext.front()};
    run_sos(sos, z, ext.data(), ext.size());
    std::reverse(ext.begin(), ext.end());
    return Signal(ext.begin() + static_cast<std::ptrdiff_t>(padlen),
                  ext.begin() + static_cast<std::ptrdiff_t>(padlen + n));
}

Signal bandpass(const Signal& x, double fs, double lo_hz, double hi_hz, int order) {
    if (x.size() < 4) return x;
    return sosfiltfilt(butter(order, BandType::BandPass, lo_hz, hi_hz, fs), x);
}

double SosFilter::step(double x) {
    double v = x;
    for (std::size_t s = 0; s < sos_.size(); ++s) {
        const Biquad& q = sos_[s];
        const double y = q.b0 * v + z_[s][0];
        z_[s][0] = q.b1 * v - q.a1 * y + z_[s][1];
        z_[s][1] = q.b2 * v - q.a2 * y;
        v = y;
    }
    return v;
}

}  // namespace rppg::dsp
