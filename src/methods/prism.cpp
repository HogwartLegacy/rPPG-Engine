// PRISM: Projection-based Robust Interpretable Signal Mixing for remote heart rate estimation
// (NeurIPS 2025, arXiv:2511.21903). Training-free: per recording (or per re-optimisation span) it
// selects the photometric detrending strength and the POS-plane mixing angle that maximise
// spectral concentration while keeping the windowed heart rate temporally smooth.
#include <algorithm>
#include <cmath>
#include <limits>

#include "rppg/methods.hpp"

namespace rppg {

using dsp::Signal;

namespace {

Signal divide_by_baseline(const Signal& x, double lambda, double fs) {
    const Signal base = dsp::smoothing_spline(x, lambda, fs);
    Signal y(x.size());
    for (std::size_t i = 0; i < x.size(); ++i) y[i] = std::abs(base[i]) > 1e-9 ? x[i] / base[i] : 1.0;
    return y;
}

Signal mix(const Signal& r, const Signal& g, const Signal& b, double alpha) {
    Signal s(g.size());
    for (std::size_t i = 0; i < g.size(); ++i) s[i] = g[i] - (alpha * b[i] + (1.0 - alpha) * r[i]);
    return dsp::remove_mean(s);
}

struct BandEval {
    double objective = std::numeric_limits<double>::infinity();
    double mean_hr = 0.0;
};

// Spectra are computed once per candidate signal and evaluated for both bands.
struct CandidateSpectra {
    dsp::Spectrum whole;
    std::vector<dsp::Spectrum> windows;
};

CandidateSpectra spectra_of(const Signal& s, double fs, double win_s, std::size_t nfft) {
    CandidateSpectra c;
    c.whole = dsp::periodogram(s, fs, std::max(nfft, dsp::next_pow2(s.size())), dsp::Window::Rect);
    const std::size_t w = static_cast<std::size_t>(std::lround(win_s * fs));
    if (w >= 8)
        for (std::size_t st = 0; st + w <= s.size(); st += w) {
            const Signal seg(s.begin() + static_cast<std::ptrdiff_t>(st),
                             s.begin() + static_cast<std::ptrdiff_t>(st + w));
            c.windows.push_back(dsp::periodogram(seg, fs, nfft, dsp::Window::Rect));
        }
    return c;
}

BandEval evaluate(const CandidateSpectra& c, double fs, double lo, double hi, double win_s, double k_tv) {
    BandEval e;
    const double total = dsp::band_power(c.whole, 1e-9, fs / 2.0);
    const double conc = total > 0 ? dsp::band_power(c.whole, lo, hi) / total : 0.0;
    std::vector<double> hr;
    for (const auto& sp : c.windows) hr.push_back(dsp::spectral_peak(sp, lo, hi).freq * 60.0);
    double tv = 0.0;
    if (hr.size() >= 2) {
        for (std::size_t i = 0; i + 1 < hr.size(); ++i) tv += std::abs(hr[i + 1] - hr[i]);
        tv /= static_cast<double>(hr.size() - 1) * win_s;
    }
    if (hr.empty()) hr.push_back(dsp::spectral_peak(c.whole, lo, hi).freq * 60.0);
    e.mean_hr = dsp::mean(hr);
    e.objective = k_tv * tv - conc;
    return e;
}

}  // namespace

Signal prism_signal(const RgbTrace& rgb, double lambda, double alpha) {
    const Signal r = divide_by_baseline(rgb.r, lambda, rgb.fs);
    const Signal g = divide_by_baseline(rgb.g, lambda, rgb.fs);
    const Signal b = divide_by_baseline(rgb.b, lambda, rgb.fs);
    return mix(r, g, b, alpha);
}

PrismChoice prism_select(const RgbTrace& rgb, const PrismParams& p) {
    PrismChoice best_h, best_l;
    best_h.objective = best_l.objective = std::numeric_limits<double>::infinity();
    if (rgb.size() < 16) return best_h;
    const double fs = rgb.fs;
    for (double lambda : p.lambdas) {
        const Signal r = divide_by_baseline(rgb.r, lambda, fs);
        const Signal g = divide_by_baseline(rgb.g, lambda, fs);
        const Signal b = divide_by_baseline(rgb.b, lambda, fs);
        for (double alpha : p.alphas) {
            const CandidateSpectra sp = spectra_of(mix(r, g, b, alpha), fs, p.window_s, p.nfft);
            const BandEval eh = evaluate(sp, fs, p.high_lo, p.high_hi, p.window_s, p.k_tv);
            const BandEval el = evaluate(sp, fs, p.low_lo, p.low_hi, p.window_s, p.k_tv);
            if (eh.objective < best_h.objective) {
                best_h = {lambda, alpha, false, eh.objective, eh.mean_hr, p.high_lo, p.high_hi};
            }
            if (el.objective < best_l.objective) {
                best_l = {lambda, alpha, true, el.objective, el.mean_hr, p.low_lo, p.low_hi};
            }
        }
    }
    // Two-band harmonic check: if the high band locked onto the second harmonic of the
    // low-band rhythm, trust the low band.
    if (best_l.mean_hr_bpm > 0 &&
        std::abs(best_h.mean_hr_bpm - 2.0 * best_l.mean_hr_bpm) <= p.harmonic_tol * 2.0 * best_l.mean_hr_bpm)
        return best_l;
    return best_h;
}

}  // namespace rppg
