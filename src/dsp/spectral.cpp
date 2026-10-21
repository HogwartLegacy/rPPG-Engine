#include <algorithm>
#include <cmath>

#include "rppg/dsp.hpp"

namespace rppg::dsp {

Signal make_window(Window w, std::size_t n) {
    Signal win(n, 1.0);
    if (n <= 1 || w == Window::Rect) return win;
    for (std::size_t i = 0; i < n; ++i) {
        // Periodic windows (scipy.signal.get_window default for spectral analysis).
        const double x = 2.0 * M_PI * static_cast<double>(i) / static_cast<double>(n);
        win[i] = (w == Window::Hann) ? 0.5 - 0.5 * std::cos(x) : 0.54 - 0.46 * std::cos(x);
    }
    return win;
}

namespace {

void one_sided_density(Signal& p, double fs, double wss, std::size_t nfft) {
    const double scale = 1.0 / (fs * wss);
    for (std::size_t k = 0; k < p.size(); ++k) {
        p[k] *= scale;
        const bool edge = (k == 0) || (nfft % 2 == 0 && k == p.size() - 1);
        if (!edge) p[k] *= 2.0;
    }
}

Signal freqs(std::size_t nfft, double fs) {
    Signal f(nfft / 2 + 1);
    for (std::size_t k = 0; k < f.size(); ++k) f[k] = fs * static_cast<double>(k) / static_cast<double>(nfft);
    return f;
}

}  // namespace

Spectrum periodogram(const Signal& x, double fs, std::size_t nfft, Window w) {
    Spectrum s;
    const std::size_t n = x.size();
    if (n == 0) return s;
    nfft = std::max(nfft, n);
    const Signal win = make_window(w, n);
    double wss = 0.0;
    Signal xw(n);
    const double m = mean(x);
    for (std::size_t i = 0; i < n; ++i) {
        xw[i] = (x[i] - m) * win[i];
        wss += win[i] * win[i];
    }
    s.power = power_spectrum(xw, nfft);
    one_sided_density(s.power, fs, wss, nfft);
    s.freq = freqs(nfft, fs);
    return s;
}

Spectrum welch(const Signal& x, double fs, std::size_t nperseg, std::size_t noverlap, std::size_t nfft, Window w) {
    const std::size_t n = x.size();
    if (n == 0) return {};
    nperseg = std::min(nperseg, n);
    if (noverlap >= nperseg) noverlap = nperseg / 2;
    nfft = std::max(nfft, nperseg);
    const std::size_t step = nperseg - noverlap;
    const Signal win = make_window(w, nperseg);
    double wss = 0.0;
    for (double v : win) wss += v * v;
    Spectrum s;
    s.freq = freqs(nfft, fs);
    s.power.assign(nfft / 2 + 1, 0.0);
    std::size_t count = 0;
    Signal seg(nperseg);
    for (std::size_t start = 0; start + nperseg <= n; start += step) {
        double m = 0.0;
        for (std::size_t i = 0; i < nperseg; ++i) m += x[start + i];
        m /= static_cast<double>(nperseg);
        for (std::size_t i = 0; i < nperseg; ++i) seg[i] = (x[start + i] - m) * win[i];
        const Signal p = power_spectrum(seg, nfft);
        for (std::size_t k = 0; k < p.size(); ++k) s.power[k] += p[k];
        ++count;
    }
    if (count == 0) return periodogram(x, fs, nfft, w);
    for (auto& v : s.power) v /= static_cast<double>(count);
    one_sided_density(s.power, fs, wss, nfft);
    return s;
}

Peak spectral_peak(const Spectrum& s, double lo, double hi) {
    Peak best;
    best.power = -1.0;
    for (std::size_t k = 0; k < s.freq.size(); ++k) {
        if (s.freq[k] < lo || s.freq[k] > hi) continue;
        if (s.power[k] > best.power) {
            best.power = s.power[k];
            best.index = k;
            best.freq = s.freq[k];
        }
    }
    if (best.power < 0.0) return Peak{};
    const std::size_t k = best.index;
    if (k > 0 && k + 1 < s.power.size()) {
        const double a = s.power[k - 1], b = s.power[k], c = s.power[k + 1];
        const double den = a - 2.0 * b + c;
        if (std::abs(den) > 1e-300) {
            const double d = std::clamp(0.5 * (a - c) / den, -0.5, 0.5);
            best.freq = s.freq[k] + d * s.df();
            best.power = b - 0.25 * (a - c) * d;
        }
    }
    return best;
}

std::vector<Peak> spectral_peaks(const Spectrum& s, double lo, double hi, std::size_t max_peaks) {
    std::vector<Peak> peaks;
    const std::size_t n = s.power.size();
    for (std::size_t k = 1; k + 1 < n; ++k) {
        if (s.freq[k] < lo || s.freq[k] > hi) continue;
        if (s.power[k] >= s.power[k - 1] && s.power[k] > s.power[k + 1]) {
            Peak p;
            p.index = k;
            const double a = s.power[k - 1], b = s.power[k], c = s.power[k + 1];
            const double den = a - 2.0 * b + c;
            const double d = std::abs(den) > 1e-300 ? std::clamp(0.5 * (a - c) / den, -0.5, 0.5) : 0.0;
            p.freq = s.freq[k] + d * s.df();
            p.power = b - 0.25 * (a - c) * d;
            peaks.push_back(p);
        }
    }
    std::sort(peaks.begin(), peaks.end(), [](const Peak& a, const Peak& b) { return a.power > b.power; });
    if (peaks.size() > max_peaks) peaks.resize(max_peaks);
    return peaks;
}

double band_power(const Spectrum& s, double lo, double hi) {
    double acc = 0.0;
    for (std::size_t k = 0; k < s.freq.size(); ++k)
        if (s.freq[k] >= lo && s.freq[k] <= hi) acc += s.power[k];
    return acc;
}

double spectrum_at(const Spectrum& s, double f) {
    if (s.freq.empty()) return 0.0;
    const double df = s.df();
    if (df <= 0.0) return s.power[0];
    const double pos = f / df;
    if (pos <= 0.0) return s.power.front();
    const std::size_t k = static_cast<std::size_t>(pos);
    if (k + 1 >= s.power.size()) return s.power.back();
    const double t = pos - static_cast<double>(k);
    return s.power[k] * (1.0 - t) + s.power[k + 1] * t;
}

double snr_db(const Spectrum& s, double f0, double lo, double hi, double dev) {
    double sig = 0.0, rest = 0.0;
    for (std::size_t k = 0; k < s.freq.size(); ++k) {
        const double f = s.freq[k];
        const bool h1 = std::abs(f - f0) <= dev;
        const bool h2 = std::abs(f - 2.0 * f0) <= dev;
        if (h1 || h2)
            sig += s.power[k];
        else if (f >= lo && f <= hi)
            rest += s.power[k];
    }
    if (rest <= 0.0 || sig <= 0.0) return sig > 0.0 ? 30.0 : -30.0;
    return 10.0 * std::log10(sig / rest);
}

Spectrum lomb_scargle(const Signal& t, const Signal& y, const Signal& freqs_hz) {
    Spectrum s;
    s.freq = freqs_hz;
    s.power.assign(freqs_hz.size(), 0.0);
    const std::size_t n = std::min(t.size(), y.size());
    if (n < 3) return s;
    const double my = mean(y);
    double var = 0.0;
    for (std::size_t i = 0; i < n; ++i) var += (y[i] - my) * (y[i] - my);
    var /= static_cast<double>(n - 1);
    if (var <= 0.0) return s;
    for (std::size_t k = 0; k < freqs_hz.size(); ++k) {
        const double w = 2.0 * M_PI * freqs_hz[k];
        if (w <= 0.0) continue;
        double s2 = 0.0, c2 = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            s2 += std::sin(2.0 * w * t[i]);
            c2 += std::cos(2.0 * w * t[i]);
        }
        const double tau = std::atan2(s2, c2) / (2.0 * w);
        double yc = 0.0, ys = 0.0, cc = 0.0, ss = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            const double a = w * (t[i] - tau);
            const double c = std::cos(a), sn = std::sin(a);
            const double d = y[i] - my;
            yc += d * c;
            ys += d * sn;
            cc += c * c;
            ss += sn * sn;
        }
        double p = 0.0;
        if (cc > 0.0) p += yc * yc / cc;
        if (ss > 0.0) p += ys * ys / ss;
        s.power[k] = 0.5 * p / var;
    }
    return s;
}

}  // namespace rppg::dsp
