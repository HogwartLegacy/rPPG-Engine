// Pulse-rate variability and respiration from the fused pulse waveform.
#include <algorithm>
#include <cmath>

#include "engine/analysis.hpp"

namespace rppg::engine {

using dsp::Signal;

namespace {

struct Beats {
    Signal t;    // beat times (s)
    Signal amp;  // peak amplitude
};

Beats detect_beats(const Signal& x, double fs, double hr_hint_bpm) {
    Beats b;
    if (x.size() < 3 * fs) return b;
    const double hr = std::clamp(hr_hint_bpm > 0 ? hr_hint_bpm : 75.0, 40.0, 200.0);
    const std::size_t min_dist = static_cast<std::size_t>(std::max(2.0, 0.6 * 60.0 / hr * fs));
    const auto pk = dsp::find_peaks(x, min_dist, 0.3 * dsp::stdev(x));
    for (std::size_t p : pk) {
        b.t.push_back(dsp::refine_peak(x, p) / fs);
        b.amp.push_back(x[p]);
    }
    return b;
}

}  // namespace

HrvMetrics hrv_from_bvp(const Signal& bvp, double fs, double hr_hint_bpm) {
    HrvMetrics m;
    const Beats b = detect_beats(bvp, fs, hr_hint_bpm);
    if (b.t.size() < 12) return m;
    Signal ibi, it;
    for (std::size_t i = 1; i < b.t.size(); ++i) {
        ibi.push_back((b.t[i] - b.t[i - 1]) * 1000.0);
        it.push_back(b.t[i]);
    }
    // Artefact rejection: physiological range and deviation from the local median.
    std::vector<char> ok(ibi.size(), 0);
    std::size_t good = 0;
    for (std::size_t i = 0; i < ibi.size(); ++i) {
        const std::size_t a = i >= 3 ? i - 3 : 0, e = std::min(ibi.size(), i + 4);
        const double med = dsp::median(Signal(ibi.begin() + static_cast<std::ptrdiff_t>(a),
                                              ibi.begin() + static_cast<std::ptrdiff_t>(e)));
        ok[i] = ibi[i] >= 300 && ibi[i] <= 1500 && std::abs(ibi[i] - med) <= 0.2 * med;
        good += ok[i];
    }
    if (good < 10 || good * 10 < ibi.size() * 7) return m;
    Signal clean, ct;
    double ss = 0;
    std::size_t nd = 0, n50 = 0;
    for (std::size_t i = 0; i < ibi.size(); ++i) {
        if (!ok[i]) continue;
        clean.push_back(ibi[i]);
        ct.push_back(it[i]);
        if (i > 0 && ok[i - 1]) {
            const double d = ibi[i] - ibi[i - 1];
            ss += d * d;
            ++nd;
            n50 += std::abs(d) > 50.0;
        }
    }
    m.beats = static_cast<int>(b.t.size());
    m.mean_ibi_ms = dsp::mean(clean);
    m.mean_hr_bpm = 60000.0 / m.mean_ibi_ms;
    m.sdnn_ms = dsp::stdev(clean, 1);
    m.rmssd_ms = nd ? std::sqrt(ss / static_cast<double>(nd)) : 0.0;
    m.pnn50 = nd ? static_cast<double>(n50) / static_cast<double>(nd) : 0.0;
    Signal freqs;
    for (double f = 0.04; f <= 0.4 + 1e-9; f += 0.005) freqs.push_back(f);
    const auto ls = dsp::lomb_scargle(ct, clean, freqs);
    m.lf = dsp::band_power(ls, 0.04, 0.15);
    m.hf = dsp::band_power(ls, 0.15, 0.4);
    m.lf_hf = m.hf > 0 ? m.lf / m.hf : 0.0;
    m.valid = true;
    return m;
}

double respiration_rate(const SignalSet& s, std::size_t k0, std::size_t k1, const Signal& bvp, double hr_bpm,
                        double& confidence) {
    confidence = 0;
    const double fs = s.fs;
    if (k1 <= k0 || static_cast<double>(k1 - k0) < 20.0 * fs) return 0.0;
    const double lo = 0.1, hi = 0.7;  // 6 - 42 breaths / min
    std::vector<double> grid;
    for (double b = 6.0; b <= 42.0 + 1e-9; b += 0.25) grid.push_back(b);
    std::vector<double> fused(grid.size(), 0.0);
    double wsum = 0;
    auto add = [&](const Signal& x, double f) {
        if (x.size() < 16 || dsp::stdev(x) <= 0) return;
        const Signal y = dsp::bandpass(dsp::remove_mean(x), f, lo, hi, 2);
        const auto sp = dsp::periodogram(y, f, std::max<std::size_t>(4096, dsp::next_pow2(4 * y.size())), dsp::Window::Hann);
        const double band = dsp::band_power(sp, lo, hi);
        if (band <= 0) return;
        const dsp::Peak pk = dsp::spectral_peak(sp, lo, hi);
        const double prom = dsp::band_power(sp, pk.freq - 0.03, pk.freq + 0.03) / band;  // peak sharpness
        std::vector<double> g(grid.size());
        double gs = 0;
        for (std::size_t j = 0; j < grid.size(); ++j) {
            g[j] = std::max(0.0, dsp::spectrum_at(sp, grid[j] / 60.0));
            gs += g[j];
        }
        if (gs <= 0) return;
        for (std::size_t j = 0; j < grid.size(); ++j) fused[j] += prom * g[j] / gs;
        wsum += prom;
    };
    auto seg = [&](const Signal& x) {
        return x.size() >= k1 ? Signal(x.begin() + static_cast<std::ptrdiff_t>(k0), x.begin() + static_cast<std::ptrdiff_t>(k1))
                              : Signal{};
    };
    // Respiratory-induced intensity variation (skin luminance) and head motion.
    add(seg(s.luma), fs);
    add(seg(s.motion[1]), fs);
    // Respiratory-induced amplitude / frequency variation of the pulse (4 Hz beat series).
    if (bvp.size() >= static_cast<std::size_t>(20 * fs)) {
        const double hr = std::clamp(hr_bpm > 0 ? hr_bpm : 75.0, 40.0, 200.0);
        const std::size_t md = static_cast<std::size_t>(std::max(2.0, 0.6 * 60.0 / hr * fs));
        const auto pk = dsp::find_peaks(bvp, md, 0.3 * dsp::stdev(bvp));
        if (pk.size() >= 10) {
            Signal t, amp, ibi;
            for (std::size_t i = 1; i < pk.size(); ++i) {
                t.push_back(pk[i] / fs);
                amp.push_back(bvp[pk[i]]);
                ibi.push_back((pk[i] - pk[i - 1]) / fs);
            }
            const double f4 = 4.0;
            const std::size_t n4 = static_cast<std::size_t>((t.back() - t.front()) * f4);
            if (n4 > 32) {
                add(dsp::resample_uniform(t, amp, t.front(), f4, n4, dsp::Interp::Cubic), f4);
                add(dsp::resample_uniform(t, ibi, t.front(), f4, n4, dsp::Interp::Cubic), f4);
            }
        }
    }
    if (wsum <= 0) return 0.0;
    for (double& v : fused) v /= wsum;
    const std::size_t m = static_cast<std::size_t>(std::max_element(fused.begin(), fused.end()) - fused.begin());
    double mass = 0, total = 0;
    for (std::size_t j = 0; j < grid.size(); ++j) {
        total += fused[j];
        if (std::abs(grid[j] - grid[m]) <= 2.0) mass += fused[j];
    }
    confidence = total > 0 ? mass / total : 0.0;
    return grid[m];
}

}  // namespace rppg::engine
