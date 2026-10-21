#include "engine/analysis.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace rppg::engine {

using dsp::Signal;

namespace {

// Facial region groups of the multi-ROI candidate (forehead/glabella, left cheek, right cheek, nose).
const std::vector<std::vector<int>>& mr_groups() {
    static const std::vector<std::vector<int>> g = {{0, 1, 2, 3}, {21, 23}, {22, 24}, {4, 5}};
    return g;
}

const char* neural_name(NeuralModel m) {
    switch (m) {
        case NeuralModel::FacePhys: return "FacePhys";
        case NeuralModel::MeRppg: return "ME-rPPG";
        default: return "none";
    }
}

template <class F>
bool resample_valid(const std::vector<FrameRecord>& rec, std::size_t i0, std::size_t i1, double t0, double fs,
                    std::size_t n, double min_cov, F&& value, Signal& out) {
    Signal t, y;
    t.reserve(i1 - i0);
    y.reserve(i1 - i0);
    double v = 0;
    for (std::size_t i = i0; i < i1; ++i)
        if (value(rec[i], v)) {
            t.push_back(rec[i].t);
            y.push_back(v);
        }
    if (t.size() < 8 || static_cast<double>(t.size()) < min_cov * static_cast<double>(i1 - i0)) return false;
    out = dsp::resample_uniform(t, y, t0, fs, n, dsp::Interp::Linear);
    return true;
}

bool group_rgb(const FrameRecord& r, const std::vector<int>& regions, int c, double& v) {
    if (!r.face) return false;
    double acc = 0, cnt = 0;
    for (int g : regions) {
        const auto n = r.n[static_cast<std::size_t>(g)];
        if (!n) continue;
        acc += static_cast<double>(n) * r.rgb[static_cast<std::size_t>(g)][static_cast<std::size_t>(c)];
        cnt += n;
    }
    if (cnt <= 0) return false;
    v = acc / cnt;
    return true;
}

// Detrend (smoothness priors, cut-off ~0.45 Hz), band-pass to the pulse band and z-score.
Signal condition(const Signal& x, double fs, double lo, double hi) {
    if (x.size() < 16) return {};
    const double w = 2.0 * M_PI * 0.45 / fs;
    Signal y = dsp::whittaker_smooth(x, std::pow(w, -4.0));
    for (std::size_t i = 0; i < x.size(); ++i) y[i] = x[i] - y[i];
    y = dsp::bandpass(y, fs, lo, std::min(hi, 0.45 * fs), 2);
    return dsp::zscore(y);
}

struct Spec {
    dsp::Spectrum s;
    std::vector<double> grid;  // normalised PSD on the HR grid
};

Spec spectrum_on_grid(const Signal& x, double fs, const std::vector<double>& grid_bpm) {
    Spec r;
    const std::size_t nfft = std::max<std::size_t>(2048, dsp::next_pow2(8 * x.size()));
    r.s = dsp::periodogram(x, fs, nfft, dsp::Window::Hann);
    r.grid.resize(grid_bpm.size());
    double sum = 0;
    for (std::size_t j = 0; j < grid_bpm.size(); ++j) {
        r.grid[j] = std::max(0.0, dsp::spectrum_at(r.s, grid_bpm[j] / 60.0));
        sum += r.grid[j];
    }
    if (sum > 0)
        for (double& v : r.grid) v /= sum;
    return r;
}

// Power within +-dev of f0 and 2 f0 over the remaining power in [lo, hi'] (linear ratio).
double snr_linear(const dsp::Spectrum& s, double f0, double lo, double hi, double dev = 0.1) {
    double sig = 0, rest = 0;
    const double top = std::max(hi, 2.0 * f0 + dev);
    for (std::size_t k = 0; k < s.freq.size(); ++k) {
        const double f = s.freq[k];
        if (f < lo - dev || f > top) continue;
        if (std::abs(f - f0) <= dev || std::abs(f - 2.0 * f0) <= dev)
            sig += s.power[k];
        else if (f >= lo)
            rest += s.power[k];
    }
    if (rest <= 0) return sig > 0 ? 1e3 : 1e-3;
    return std::max(sig / rest, 1e-3);
}

std::size_t argmax(const std::vector<double>& v) {
    return static_cast<std::size_t>(std::max_element(v.begin(), v.end()) - v.begin());
}

double refined_peak_bpm(const std::vector<double>& g, const std::vector<double>& p) {
    const std::size_t k = argmax(p);
    if (k == 0 || k + 1 >= p.size()) return g[k];
    const double a = p[k - 1], b = p[k], c = p[k + 1];
    const double den = a - 2 * b + c;
    const double d = std::abs(den) > 1e-300 ? std::clamp(0.5 * (a - c) / den, -0.5, 0.5) : 0.0;
    return g[k] + d * (g[1] - g[0]);
}

// Lag (|lag| <= max_lag samples) and sign maximising the correlation of y with ref.
void align(const Signal& ref, Signal& y, std::size_t max_lag) {
    const std::size_t n = std::min(ref.size(), y.size());
    double best = 0;
    long best_lag = 0;
    for (long lag = -static_cast<long>(max_lag); lag <= static_cast<long>(max_lag); ++lag) {
        double c = 0;
        for (std::size_t i = 0; i < n; ++i) {
            const long j = static_cast<long>(i) + lag;
            if (j < 0 || j >= static_cast<long>(n)) continue;
            c += ref[i] * y[static_cast<std::size_t>(j)];
        }
        if (std::abs(c) > std::abs(best)) {
            best = c;
            best_lag = lag;
        }
    }
    Signal out(n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        const long j = static_cast<long>(i) + best_lag;
        out[i] = y[static_cast<std::size_t>(std::clamp<long>(j, 0, static_cast<long>(n) - 1))];
    }
    if (best < 0)
        for (double& v : out) v = -v;
    y.swap(out);
}

Signal slice(const Signal& x, std::size_t k0, std::size_t k1) {
    return Signal(x.begin() + static_cast<std::ptrdiff_t>(k0), x.begin() + static_cast<std::ptrdiff_t>(k1));
}

double logistic(double x) { return 1.0 / (1.0 + std::exp(-x)); }

}  // namespace

Analyzer::Analyzer(const EngineConfig& cfg) : cfg_(cfg) {
    lo_hz_ = cfg.hr_min_bpm / 60.0;
    hi_hz_ = cfg.hr_max_bpm / 60.0;
    for (double b = cfg.hr_min_bpm; b <= cfg.hr_max_bpm + 1e-9; b += 0.5) grid_.push_back(b);
}

std::vector<std::string> Analyzer::candidate_names() const {
    std::vector<std::string> n;
    for (Method m : cfg_.methods) n.push_back(method_name(m));
    if (cfg_.multi_roi) n.push_back("POS-MR");
    for (NeuralModel m : cfg_.neural)
        if (neural_slot(m) >= 0) n.push_back(neural_name(m));
    return n;
}

SignalSet Analyzer::signals(const std::vector<FrameRecord>& rec, std::size_t i0, std::size_t i1, PrismCache* prism,
                            bool reselect_prism) const {
    SignalSet s;
    s.fs = cfg_.fs;
    s.names = candidate_names();
    for (NeuralModel m : cfg_.neural) s.n_neural += neural_slot(m) >= 0 ? 1 : 0;
    s.bvp.assign(s.names.size(), {});
    s.parts.assign(s.names.size(), {});
    if (i1 <= i0 + 8) return s;
    s.t0 = rec[i0].t;
    const double dur = rec[i1 - 1].t - s.t0;
    s.n = static_cast<std::size_t>(std::floor(dur * s.fs)) + 1;
    const double fs = s.fs, t0 = s.t0;
    const std::size_t n = s.n;
    const double cov = 0.6;

    // Face presence and motion signals.
    s.face.assign(n, 0.0);
    {
        Signal t, y;
        for (std::size_t i = i0; i < i1; ++i) {
            t.push_back(rec[i].t);
            y.push_back(rec[i].face ? 1.0 : 0.0);
        }
        s.face = dsp::resample_uniform(t, y, t0, fs, n, dsp::Interp::Linear);
    }
    auto mot = [&](int which) {
        return [which](const FrameRecord& r, double& v) {
            if (!r.face || r.scale <= 0) return false;
            switch (which) {
                case 0: v = r.cx / r.scale; break;
                case 1: v = r.cy / r.scale; break;
                case 2: v = std::log(r.scale); break;
                default: v = r.roll; break;
            }
            return true;
        };
    };
    for (int k = 0; k < 4; ++k)
        if (!resample_valid(rec, i0, i1, t0, fs, n, cov, mot(k), s.motion[static_cast<std::size_t>(k)]))
            s.motion[static_cast<std::size_t>(k)].assign(n, 0.0);
    if (!resample_valid(rec, i0, i1, t0, fs, n, cov,
                        [](const FrameRecord& r, double& v) {
                            if (!r.face) return false;
                            const double l = 1.0 + r.n[21] + r.n[23], rr = 1.0 + r.n[22] + r.n[24];
                            v = std::abs(std::log(l / rr));
                            return true;
                        },
                        s.asym))
        s.asym.assign(n, 0.0);

    // Holistic RGB trace (+ 2SR correlation matrices).
    RgbTrace holo;
    holo.fs = fs;
    const std::vector<int> H = {vision::kHolistic};
    bool have_holo = true;
    for (int c = 0; c < 3 && have_holo; ++c) {
        Signal& dst = c == 0 ? holo.r : (c == 1 ? holo.g : holo.b);
        have_holo = resample_valid(rec, i0, i1, t0, fs, n, cov,
                                   [&](const FrameRecord& r, double& v) { return group_rgb(r, H, c, v); }, dst);
    }
    if (have_holo) {
        std::array<Signal, 6> m2;
        bool ok = true;
        for (int k = 0; k < 6 && ok; ++k)
            ok = resample_valid(rec, i0, i1, t0, fs, n, cov,
                                [k](const FrameRecord& r, double& v) {
                                    if (!r.face || !r.n[vision::kHolistic]) return false;
                                    v = r.corr[static_cast<std::size_t>(k)];
                                    return true;
                                },
                                m2[static_cast<std::size_t>(k)]);
        if (ok) {
            holo.corr.resize(n);
            for (std::size_t i = 0; i < n; ++i) {
                dsp::Mat3& C = holo.corr[i];
                C[0][0] = m2[0][i], C[1][1] = m2[1][i], C[2][2] = m2[2][i];
                C[0][1] = C[1][0] = m2[3][i];
                C[0][2] = C[2][0] = m2[4][i];
                C[1][2] = C[2][1] = m2[5][i];
            }
        }
        s.luma = holo.g;
        s.mean_luma = dsp::mean(holo.g);
    }
    Signal bgt, bgy;
    for (std::size_t i = i0; i < i1; ++i)
        if (rec[i].bg_valid) {
            bgt.push_back(rec[i].t);
            bgy.push_back(rec[i].bg[1]);
        }
    if (bgt.size() > 8) s.bg = dsp::resample_uniform(bgt, bgy, t0, fs, n, dsp::Interp::Linear);
    double sat = 0;
    for (std::size_t i = i0; i < i1; ++i) sat += rec[i].saturated;
    s.saturated = sat / static_cast<double>(i1 - i0);

    MethodParams mp;
    mp.hr_lo_hz = lo_hz_;
    mp.hr_hi_hz = hi_hz_;
    std::size_t ci = 0;
    for (Method m : cfg_.methods) {
        if (have_holo) {
            Signal raw;
            if (m == Method::PRISM) {
                if (prism && (reselect_prism || !prism->ready)) {
                    prism->choice = prism_select(holo);
                    prism->ready = true;
                }
                const PrismChoice c = prism ? prism->choice : prism_select(holo);
                raw = prism_signal(holo, c.lambda, c.alpha);
            } else if (m != Method::SSR || !holo.corr.empty()) {
                raw = extract_bvp(m, holo, mp);
            }
            if (!raw.empty()) s.bvp[ci] = condition(raw, fs, lo_hz_, hi_hz_);
        }
        ++ci;
    }
    if (cfg_.multi_roi) {
        for (const auto& grp : mr_groups()) {
            RgbTrace t;
            t.fs = fs;
            bool ok = true;
            for (int c = 0; c < 3 && ok; ++c) {
                Signal& dst = c == 0 ? t.r : (c == 1 ? t.g : t.b);
                ok = resample_valid(rec, i0, i1, t0, fs, n, cov,
                                    [&](const FrameRecord& r, double& v) { return group_rgb(r, grp, c, v); }, dst);
            }
            if (ok) s.parts[ci].push_back(condition(extract_bvp(Method::POS, t, mp), fs, lo_hz_, hi_hz_));
        }
        if (!s.parts[ci].empty()) s.bvp[ci] = Signal(n, 0.0);  // combined per window
        ++ci;
    }
    for (NeuralModel m : cfg_.neural) {
        const int slot = neural_slot(m);
        if (slot < 0) continue;
        const auto k = static_cast<std::size_t>(slot);
        Signal raw;
        if (resample_valid(rec, i0, i1, t0, fs, n, cov,
                           [k](const FrameRecord& r, double& v) {
                               if (!r.nn_valid[k]) return false;
                               v = r.nn[k];
                               return true;
                           },
                           raw))
            s.bvp[ci] = condition(raw, fs, lo_hz_, hi_hz_);
        ++ci;
    }
    return s;
}

void Analyzer::reselect_prism(const std::vector<FrameRecord>& rec, std::size_t i0, std::size_t i1,
                              PrismCache& cache) const {
    if (i1 <= i0 + 16) return;
    const double t0 = rec[i0].t;
    const std::size_t n = static_cast<std::size_t>(std::floor((rec[i1 - 1].t - t0) * cfg_.fs)) + 1;
    RgbTrace holo;
    holo.fs = cfg_.fs;
    const std::vector<int> H = {vision::kHolistic};
    for (int c = 0; c < 3; ++c) {
        Signal& dst = c == 0 ? holo.r : (c == 1 ? holo.g : holo.b);
        if (!resample_valid(rec, i0, i1, t0, cfg_.fs, n, 0.6,
                            [&](const FrameRecord& r, double& v) { return group_rgb(r, H, c, v); }, dst))
            return;
    }
    PrismParams p;
    p.high_lo = std::max(p.high_lo, lo_hz_);
    p.high_hi = std::min(p.high_hi, hi_hz_);
    cache.choice = prism_select(holo, p);
    cache.ready = true;
}

WindowResult Analyzer::window(const SignalSet& s, std::size_t k0, std::size_t k1) const {
    WindowResult w;
    if (k1 > s.n) k1 = s.n;
    if (k1 <= k0 || static_cast<double>(k1 - k0) < 4.0 * s.fs) return w;
    const double fs = s.fs;
    const std::size_t N = k1 - k0;
    w.face_ratio = s.face.empty() ? 0.0 : dsp::mean(slice(s.face, k0, k1));

    // In-band head motion amplitude (face widths) and its spectrum.
    std::vector<double> motion_psd(grid_.size(), 0.0);
    double motion_amp = 0;
    for (int k = 0; k < 2; ++k) {
        const Signal& m = s.motion[static_cast<std::size_t>(k)];
        if (m.size() < k1) continue;
        Signal seg = dsp::bandpass(dsp::remove_mean(slice(m, k0, k1)), fs, lo_hz_, std::min(hi_hz_, 0.45 * fs), 2);
        const double sd = dsp::stdev(seg);
        motion_amp = std::max(motion_amp, sd);
        if (sd <= 0) continue;
        const Spec sp = spectrum_on_grid(seg, fs, grid_);
        for (std::size_t j = 0; j < grid_.size(); ++j) motion_psd[j] += sp.grid[j] * sd * sd;
    }
    {
        // Gross motion speed (face widths per second) for the quality index.
        double acc = 0;
        std::size_t cnt = 0;
        for (int k = 0; k < 2; ++k) {
            const Signal& m = s.motion[static_cast<std::size_t>(k)];
            if (m.size() < k1) continue;
            for (std::size_t i = k0 + 1; i < k1; ++i) {
                const double d = (m[i] - m[i - 1]) * fs;
                acc += d * d;
                ++cnt;
            }
        }
        w.motion = cnt ? std::sqrt(acc / static_cast<double>(cnt)) : 0.0;
    }

    struct C {
        std::size_t idx;
        Signal x;
        Spec sp;
        double hr = 0, snr = 0, wgt = 0;
    };
    std::vector<C> cs;
    for (std::size_t i = 0; i < s.names.size(); ++i) {
        if (s.bvp[i].empty()) continue;
        Signal x;
        if (!s.parts[i].empty()) {
            // Multi-ROI maximal-ratio combination (DistancePPG goodness weights).
            std::vector<Signal> parts;
            std::vector<Spec> ps;
            std::vector<double> sum(grid_.size(), 0.0);
            for (const auto& p : s.parts[i]) {
                parts.push_back(dsp::zscore(slice(p, k0, k1)));
                ps.push_back(spectrum_on_grid(parts.back(), fs, grid_));
                for (std::size_t j = 0; j < grid_.size(); ++j) sum[j] += ps.back().grid[j];
            }
            const double f0 = grid_[argmax(sum)] / 60.0;
            std::vector<double> g(parts.size());
            std::size_t best = 0;
            for (std::size_t k = 0; k < parts.size(); ++k) {
                const double q = snr_linear(ps[k].s, f0, lo_hz_, hi_hz_);
                g[k] = std::sqrt(q * (1.0 + q));
                if (g[k] > g[best]) best = k;
            }
            x.assign(N, 0.0);
            double gs = 0;
            for (std::size_t k = 0; k < parts.size(); ++k) {
                if (k != best) align(parts[best], parts[k], static_cast<std::size_t>(0.15 * fs));
                for (std::size_t t = 0; t < N; ++t) x[t] += g[k] * parts[k][t];
                gs += g[k];
            }
            if (gs > 0)
                for (double& v : x) v /= gs;
        } else {
            x = slice(s.bvp[i], k0, k1);
        }
        x = dsp::zscore(x);
        if (dsp::stdev(x) <= 0) continue;
        C c;
        c.idx = i;
        c.sp = spectrum_on_grid(x, fs, grid_);
        c.x = std::move(x);
        c.hr = refined_peak_bpm(grid_, c.sp.grid);
        c.snr = dsp::snr_db(c.sp.s, c.hr / 60.0, lo_hz_, hi_hz_);
        cs.push_back(std::move(c));
    }
    if (cs.empty()) return w;

    // Motion-aware attenuation of each candidate spectrum (frequencies dominated by head motion).
    double beta = 0;
    if (cfg_.motion_compensation && motion_amp > 0.01) {
        beta = std::clamp((motion_amp - 0.01) / 0.01, 0.0, 4.0);
        const double mmax = *std::max_element(motion_psd.begin(), motion_psd.end());
        if (mmax > 0)
            for (auto& c : cs) {
                double sum = 0;
                for (std::size_t j = 0; j < grid_.size(); ++j) {
                    c.sp.grid[j] /= 1.0 + beta * motion_psd[j] / mmax;
                    sum += c.sp.grid[j];
                }
                if (sum > 0)
                    for (double& v : c.sp.grid) v /= sum;
            }
    }

    // Consensus fusion: weights = prior x linear SNR of each candidate at the fused peak.
    double classical_prior = cfg_.classical_prior;
    if (cfg_.pose_aware_prior && s.asym.size() >= k1) {
        w.asymmetry = dsp::mean(slice(s.asym, k0, k1));
        classical_prior /= 1.0 + std::pow(w.asymmetry / std::max(cfg_.pose_asym_ref, 1e-3), 4.0);
    }
    if (cfg_.motion_prior_ref > 0 && s.n_neural > 0)
        classical_prior /= 1.0 + std::pow(w.motion / cfg_.motion_prior_ref, 2.0);
    std::vector<double> fused(grid_.size(), 0.0);
    for (auto& c : cs) c.wgt = 1.0;
    for (int it = 0; it < 3; ++it) {
        std::fill(fused.begin(), fused.end(), 0.0);
        double ws = 0;
        for (const auto& c : cs) {
            for (std::size_t j = 0; j < grid_.size(); ++j) fused[j] += c.wgt * c.sp.grid[j];
            ws += c.wgt;
        }
        if (ws > 0)
            for (double& v : fused) v /= ws;
        if (!cfg_.fuse) break;
        const double f0 = grid_[argmax(fused)] / 60.0;
        for (auto& c : cs) {
            const bool neural = c.idx >= s.names.size() - s.n_neural;
            c.wgt = (neural ? 1.0 : classical_prior) *
                    std::pow(snr_linear(c.sp.s, f0, lo_hz_, hi_hz_), cfg_.fusion_power);
        }
    }
    if (!cfg_.fuse) {
        // Single candidate mode: the first available candidate decides.
        fused = cs.front().sp.grid;
        for (std::size_t k = 1; k < cs.size(); ++k) cs[k].wgt = 0;
        cs.front().wgt = 1;
    }
    w.fused = fused;
    w.hr_raw = refined_peak_bpm(grid_, fused);

    // Fused waveform: candidates aligned to the strongest one.
    std::size_t ref = 0;
    for (std::size_t k = 1; k < cs.size(); ++k)
        if (cs[k].wgt > cs[ref].wgt) ref = k;
    w.bvp.assign(N, 0.0);
    double ws = 0;
    for (std::size_t k = 0; k < cs.size(); ++k) {
        if (cs[k].wgt <= 0) continue;
        Signal y = cs[k].x;
        if (k != ref) align(cs[ref].x, y, static_cast<std::size_t>(0.3 * fs));
        for (std::size_t t = 0; t < N; ++t) w.bvp[t] += cs[k].wgt * y[t];
        ws += cs[k].wgt;
    }
    w.bvp = dsp::zscore(w.bvp);
    const Spec fb = spectrum_on_grid(w.bvp, fs, grid_);
    w.snr_db = dsp::snr_db(fb.s, w.hr_raw / 60.0, lo_hz_, hi_hz_);

    double agree = 0;
    for (const auto& c : cs) {
        if (std::abs(c.hr - w.hr_raw) <= 5.0) agree += c.wgt;
        w.cand.push_back({s.names[c.idx], c.hr, c.snr, ws > 0 ? c.wgt / ws : 0.0});
    }
    w.agreement = ws > 0 ? agree / ws : 0.0;
    const double q_snr = logistic((w.snr_db + 3.0) / 2.0);
    const double q_motion = 1.0 / (1.0 + std::pow(w.motion / 0.6, 2.0));
    w.sqi = q_snr * (0.4 + 0.6 * w.agreement) * std::clamp(w.face_ratio, 0.0, 1.0) * q_motion;
    w.valid = w.face_ratio > 0.5;
    return w;
}

// ------------------------------------------------------------------------------------------------
HrTracker::HrTracker(const std::vector<double>& grid_bpm, double sigma_bpm_per_s, double jump_prob)
    : grid_(grid_bpm), sigma_(sigma_bpm_per_s), jump_(jump_prob) {}

void HrTracker::reset() { init_ = false; }

namespace {
std::vector<double> gaussian_kernel(double sigma_bins, int& half) {
    half = std::max(1, static_cast<int>(std::ceil(4 * sigma_bins)));
    std::vector<double> k(static_cast<std::size_t>(2 * half + 1));
    double s = 0;
    for (int i = -half; i <= half; ++i) {
        k[static_cast<std::size_t>(i + half)] = std::exp(-0.5 * (i / sigma_bins) * (i / sigma_bins));
        s += k[static_cast<std::size_t>(i + half)];
    }
    for (double& v : k) v /= s;
    return k;
}

std::vector<double> emission(const std::vector<double>& obs, double quality) {
    const double mx = *std::max_element(obs.begin(), obs.end());
    std::vector<double> e(obs.size(), 1.0);
    if (mx <= 0) return e;
    const double kappa = 1.0 + 3.0 * std::clamp(quality, 0.0, 1.0);
    for (std::size_t j = 0; j < obs.size(); ++j) e[j] = 0.02 + std::pow(obs[j] / mx, kappa);
    return e;
}
}  // namespace

void HrTracker::update(const std::vector<double>& obs, double dt, double quality, double& hr, double& confidence) {
    const std::size_t n = grid_.size();
    const double step = grid_.size() > 1 ? grid_[1] - grid_[0] : 1.0;
    const std::vector<double> e = emission(obs, quality);
    if (!init_ || post_.size() != n) {
        post_.assign(n, 1.0 / static_cast<double>(n));
        init_ = true;
    } else {
        int half = 0;
        const auto k = gaussian_kernel(std::max(0.5, sigma_ * std::max(dt, 0.1) / step), half);
        std::vector<double> pred(n, 0.0);
        for (std::size_t j = 0; j < n; ++j) {
            double acc = 0;
            for (int d = -half; d <= half; ++d) {
                const long i = static_cast<long>(j) + d;
                if (i < 0 || i >= static_cast<long>(n)) continue;
                acc += post_[static_cast<std::size_t>(i)] * k[static_cast<std::size_t>(d + half)];
            }
            pred[j] = (1.0 - jump_) * acc + jump_ / static_cast<double>(n);
        }
        post_.swap(pred);
    }
    double s = 0;
    for (std::size_t j = 0; j < n; ++j) {
        post_[j] *= e[j];
        s += post_[j];
    }
    if (s <= 0) {
        post_.assign(n, 1.0 / static_cast<double>(n));
        s = 1;
    }
    for (double& v : post_) v /= s;
    const std::size_t m = argmax(post_);
    double mass = 0;
    for (std::size_t j = 0; j < n; ++j)
        if (std::abs(grid_[j] - grid_[m]) <= 5.0) mass += post_[j];
    // Report the observation's local spectral maximum near the posterior mode (sub-grid precision).
    std::size_t k = m;
    const std::size_t lo = m >= 4 ? m - 4 : 0, hi = std::min(n - 1, m + 4);
    for (std::size_t j = lo; j <= hi; ++j)
        if (obs[j] > obs[k]) k = j;
    hr = grid_[k];
    if (k > 0 && k + 1 < n) {
        const double a = obs[k - 1], b = obs[k], c = obs[k + 1], den = a - 2 * b + c;
        if (std::abs(den) > 1e-300) hr += std::clamp(0.5 * (a - c) / den, -0.5, 0.5) * step;
    }
    confidence = mass;
}

std::vector<double> HrTracker::viterbi(const std::vector<double>& grid, const std::vector<std::vector<double>>& obs,
                                       const std::vector<double>& quality, double dt, double sigma, double jump) {
    const std::size_t T = obs.size(), n = grid.size();
    std::vector<double> path(T, 0.0);
    if (T == 0 || n == 0) return path;
    const double step = n > 1 ? grid[1] - grid[0] : 1.0;
    const double sb = std::max(0.5, sigma * dt / step);
    const int half = std::max(1, static_cast<int>(std::ceil(4 * sb)));
    std::vector<double> logk(static_cast<std::size_t>(2 * half + 1));
    for (int d = -half; d <= half; ++d)
        logk[static_cast<std::size_t>(d + half)] = std::log(1.0 - jump) - 0.5 * (d / sb) * (d / sb) -
                                                   std::log(sb * std::sqrt(2.0 * M_PI));
    const double log_jump = std::log(jump / static_cast<double>(n));
    std::vector<double> delta(n), next(n);
    std::vector<std::vector<int>> back(T, std::vector<int>(n, 0));
    auto loge = [&](std::size_t t) {
        const auto e = emission(obs[t], quality[t]);
        std::vector<double> l(n);
        for (std::size_t j = 0; j < n; ++j) l[j] = std::log(e[j]);
        return l;
    };
    std::vector<double> le = loge(0);
    for (std::size_t j = 0; j < n; ++j) delta[j] = le[j];
    for (std::size_t t = 1; t < T; ++t) {
        le = loge(t);
        const std::size_t best_prev = argmax(delta);
        for (std::size_t j = 0; j < n; ++j) {
            double best = delta[best_prev] + log_jump;
            int arg = static_cast<int>(best_prev);
            for (int d = -half; d <= half; ++d) {
                const long i = static_cast<long>(j) + d;
                if (i < 0 || i >= static_cast<long>(n)) continue;
                const double v = delta[static_cast<std::size_t>(i)] + logk[static_cast<std::size_t>(d + half)];
                if (v > best) {
                    best = v;
                    arg = static_cast<int>(i);
                }
            }
            next[j] = best + le[j];
            back[t][j] = arg;
        }
        const double mx = *std::max_element(next.begin(), next.end());
        for (std::size_t j = 0; j < n; ++j) delta[j] = next[j] - mx;
    }
    std::size_t cur = argmax(delta);
    for (std::size_t t = T; t-- > 0;) {
        path[t] = grid[cur];
        if (t > 0) cur = static_cast<std::size_t>(back[t][cur]);
    }
    return path;
}

}  // namespace rppg::engine
