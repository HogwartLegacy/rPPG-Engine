#include "rppg/engine.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <type_traits>

#include "engine/analysis.hpp"
#include "nn/stream_rppg.hpp"
#include "vision/face.hpp"
#include "vision/roi.hpp"

namespace rppg {

const char* version() { return "1.0.0"; }

HrvMetrics hrv_from_pulse(const std::vector<double>& pulse, double fs, double hr_hint_bpm) {
    return engine::hrv_from_bvp(pulse, fs, hr_hint_bpm);
}

namespace {

std::string join(const std::string& dir, const std::string& file) {
    if (dir.empty()) return file;
    const char last = dir.back();
    return (last == '/' || last == '\\') ? dir + file : dir + "/" + file;
}

struct Pose {
    double cx = 0, cy = 0, scale = 0, roll = 0;
};

Pose pose_of(const std::vector<Point2f>& L) {
    Pose p;
    if (L.size() < 468) return p;
    double sx = 0, sy = 0;
    for (std::size_t i = 0; i < 468; ++i) {
        sx += L[i].x;
        sy += L[i].y;
    }
    p.cx = sx / 468.0;
    p.cy = sy / 468.0;
    p.scale = std::hypot(double(L[263].x) - L[33].x, double(L[263].y) - L[33].y);
    p.roll = std::atan2(double(L[263].y) - L[33].y, double(L[263].x) - L[33].x);
    return p;
}

static_assert(std::is_trivially_copyable<engine::FrameRecord>::value, "FrameRecord must be POD for sessions");
constexpr char kSessionMagic[8] = {'R', 'P', 'P', 'G', 'S', 'E', 'S', '1'};

}  // namespace

struct Engine::Impl {
    EngineConfig cfg;
    std::unique_ptr<vision::FaceTracker> tracker;
    vision::RoiSampler sampler;
    std::array<std::unique_ptr<nn::StreamRppg>, engine::kMaxNeural> nets;
    std::array<double, engine::kMaxNeural> net_last_t{{-1.0, -1.0}};
    engine::Analyzer analyzer;
    engine::HrTracker hr;
    std::vector<engine::FrameRecord> rec;
    vision::RgbImage rgb;
    std::vector<float> crop;
    engine::PrismCache prism;
    VitalSigns vit;
    double last_hop = -1e9, last_long = -1e9, t_start = 0;
    bool started = false;
    HrvMetrics hrv;
    double rr = 0, rr_conf = 0;

    explicit Impl(const EngineConfig& c) : cfg(c), analyzer(c), hr(analyzer.grid(), c.track_sigma_bpm_per_s) {
        crop.resize(36 * 36 * 3);
        if (!cfg.load_models) return;
        if (cfg.use_face_mesh) {
            vision::TrackerConfig tc;
            tc.detector_model = join(cfg.model_dir, "mediapipe_face_detector.onnx");
            tc.landmark_model = join(cfg.model_dir, "mediapipe_face_landmarks.onnx");
            tc.threads = cfg.threads;
            tc.smooth_min_cutoff = cfg.smooth_min_cutoff;
            tc.smooth_beta = cfg.smooth_beta;
            tc.redetect_interval = cfg.redetect_interval;
            tracker = std::make_unique<vision::FaceTracker>(tc);
        }
        for (NeuralModel m : cfg.neural) {
            const int slot = engine::neural_slot(m);
            if (slot < 0 || nets[static_cast<std::size_t>(slot)]) continue;
            const std::string base = m == NeuralModel::FacePhys ? "facephys" : "me_rppg";
            nets[static_cast<std::size_t>(slot)] = std::make_unique<nn::StreamRppg>(
                join(cfg.model_dir, base + ".onnx"), join(cfg.model_dir, base + ".state.bin"), cfg.threads);
        }
    }

    void trim() {
        if (cfg.keep_all_frames || rec.empty()) return;
        const double oldest = rec.back().t - cfg.max_history_s;
        std::size_t k = 0;
        while (k < rec.size() && rec[k].t < oldest) ++k;
        if (k > 256 || (k > 0 && k * 4 > rec.size())) rec.erase(rec.begin(), rec.begin() + static_cast<std::ptrdiff_t>(k));
    }

    std::size_t first_after(double t, std::size_t end) const {
        const auto it = std::lower_bound(rec.begin(), rec.begin() + static_cast<std::ptrdiff_t>(end), t,
                                         [](const engine::FrameRecord& r, double v) { return r.t < v; });
        return static_cast<std::size_t>(it - rec.begin());
    }

    void run_neural(const std::vector<Point2f>& lm, double t, engine::FrameRecord& r) {
        if (!nets[0] && !nets[1]) return;
        // ME-rPPG / FacePhys input: bounding box of the face mesh, area-resized to 36 x 36 RGB.
        float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
        for (const auto& p : lm) {
            x0 = std::min(x0, p.x);
            y0 = std::min(y0, p.y);
            x1 = std::max(x1, p.x);
            y1 = std::max(y1, p.y);
        }
        vision::crop_resize_area(rgb, x0, y0, x1, y1, 36, 36, crop.data());
        for (std::size_t k = 0; k < nets.size(); ++k) {
            if (!nets[k]) continue;
            double dt = net_last_t[k] < 0 ? 1.0 / 30.0 : t - net_last_t[k];
            if (dt > 2.0) {  // long gap: restart the recurrent state
                nets[k]->reset();
                dt = 1.0 / 30.0;
            }
            dt = std::clamp(dt, 1.0 / 90.0, 0.5);
            net_last_t[k] = t;
            r.nn[k] = nets[k]->step(crop.data(), static_cast<float>(dt));
            r.nn_valid[k] = std::isfinite(r.nn[k]);
        }
    }

    // Causal analysis at time t using records [.., end).
    void streaming_update(double t, std::size_t end) {
        const double W = cfg.window_s;
        const std::size_t i0 = first_after(t - W - 2.0, end);
        VitalSigns v;
        v.timestamp = t;
        if (end < i0 + 16 || t - rec[i0].t < 0.5 * W) {
            vit = v;
            return;
        }
        const bool want_prism = std::find(cfg.methods.begin(), cfg.methods.end(), Method::PRISM) != cfg.methods.end();
        if (want_prism && t - prism.selected_at >= 10.0) {
            // PRISM re-optimises its projection on the last minute of data.
            analyzer.reselect_prism(rec, first_after(t - 60.0, end), end, prism);
            prism.selected_at = t;
        }
        const engine::SignalSet s = analyzer.signals(rec, i0, end, &prism, false);
        const std::size_t kw = static_cast<std::size_t>(W * s.fs);
        const std::size_t k1 = s.n, k0 = k1 > kw ? k1 - kw : 0;
        const engine::WindowResult w = analyzer.window(s, k0, k1);
        v.face_ratio = w.face_ratio;
        v.motion = w.motion;
        v.low_light = s.mean_luma > 0 && s.mean_luma < 40.0;
        v.saturation = s.saturated > 0.05;
        if (!w.fused.empty()) {
            v.hr_raw_bpm = w.hr_raw;
            v.snr_db = w.snr_db;
            v.sqi = w.sqi;
            v.candidates = w.cand;
            v.bvp.assign(w.bvp.begin(), w.bvp.end());
            v.bvp_fs = s.fs;
            if (cfg.track) {
                const double dt = vit.timestamp > 0 ? std::clamp(t - vit.timestamp, 0.1, 10.0) : cfg.hop_s;
                hr.update(w.fused, dt, w.sqi, v.hr_bpm, v.confidence);
            } else {
                v.hr_bpm = w.hr_raw;
                v.confidence = w.agreement;
            }
            v.valid = w.valid;
        }
        // Long-span measures every 5 s: respiration (>= 20 s) and HRV (>= 30 s, good quality).
        if (t - last_long >= 5.0) {
            last_long = t;
            const std::size_t j0 = first_after(t - 60.0, end);
            if (t - rec[j0].t >= 20.0) {
                const engine::SignalSet L = analyzer.signals(rec, j0, end, &prism, false);
                const engine::WindowResult lw = analyzer.window(L, 0, L.n);
                if (!lw.bvp.empty()) {
                    rr = engine::respiration_rate(L, 0, L.n, lw.bvp, v.hr_bpm, rr_conf);
                    hrv = (t - rec[j0].t >= 30.0 && lw.sqi > 0.3) ? engine::hrv_from_bvp(lw.bvp, L.fs, v.hr_bpm)
                                                                 : HrvMetrics{};
                }
            }
        }
        v.rr_bpm = rr;
        v.rr_confidence = rr_conf;
        v.hrv = hrv;
        vit = std::move(v);
    }

    void reset_analysis() {
        hr.reset();
        prism = engine::PrismCache{};
        vit = VitalSigns{};
        last_hop = last_long = -1e9;
        hrv = HrvMetrics{};
        rr = rr_conf = 0;
    }
};

Engine::Engine(const EngineConfig& cfg) : impl_(std::make_unique<Impl>(cfg)) {}
Engine::~Engine() = default;
const EngineConfig& Engine::config() const { return impl_->cfg; }
std::size_t Engine::frames() const { return impl_->rec.size(); }
VitalSigns Engine::vitals() const { return impl_->vit; }

void Engine::reset() {
    auto& I = *impl_;
    if (I.tracker) I.tracker->reset();
    I.sampler.reset();
    for (auto& n : I.nets)
        if (n) n->reset();
    I.net_last_t.fill(-1.0);
    I.rec.clear();
    I.started = false;
    I.reset_analysis();
}

FrameInfo Engine::process(const ImageView& frame, double t, const std::vector<Point2f>* ext, bool want_rois) {
    auto& I = *impl_;
    if (!I.cfg.load_models && !(ext && ext->size() >= 468))
        throw std::logic_error("rppg::Engine::process: no models loaded and no external landmarks given");
    FrameInfo info;
    const int f = vision::to_rgb(frame, I.rgb, I.cfg.max_side);
    if (f <= 0) throw std::invalid_argument("rppg::Engine::process: invalid frame");
    if (!I.started) {
        I.started = true;
        I.t_start = t;
    }
    if (!I.rec.empty() && t <= I.rec.back().t) t = I.rec.back().t + 1e-4;  // enforce monotonic time

    std::vector<Point2f> lm;
    if (ext && ext->size() >= 468) {
        lm = *ext;
        for (auto& p : lm) {
            p.x /= f;
            p.y /= f;
        }
    } else if (I.tracker) {
        const auto& face = I.tracker->update(I.rgb, t);
        if (face.present) lm = face.landmarks;
    }

    engine::FrameRecord r;
    r.t = t;
    if (lm.size() >= 468) {
        vision::FrameRegions fr;
        I.sampler.sample(I.rgb, lm, fr);
        for (int k = 0; k < vision::kNumRegions; ++k) {
            const auto& st = fr.region[static_cast<std::size_t>(k)];
            if (!st.valid) continue;
            r.n[static_cast<std::size_t>(k)] = st.n_skin;
            for (int c = 0; c < 3; ++c)
                r.rgb[static_cast<std::size_t>(k)][static_cast<std::size_t>(c)] = static_cast<float>(st.mean(c));
        }
        const auto& h = fr.region[vision::kHolistic];
        if (h.n_skin)
            for (int k = 0; k < 6; ++k) r.corr[static_cast<std::size_t>(k)] = static_cast<float>(h.sum2[k] / h.n_skin);
        for (int c = 0; c < 3; ++c) r.bg[static_cast<std::size_t>(c)] = static_cast<float>(fr.background[c]);
        r.bg_valid = fr.background_valid;
        r.luma = static_cast<float>(fr.face_luma);
        r.saturated = static_cast<float>(fr.saturated_fraction);
        const Pose p = pose_of(lm);
        r.cx = static_cast<float>(p.cx);
        r.cy = static_cast<float>(p.cy);
        r.scale = static_cast<float>(p.scale);
        r.roll = static_cast<float>(p.roll);
        r.face = r.n[vision::kHolistic] > 0;
        if (r.face) I.run_neural(lm, t, r);
    }
    I.rec.push_back(r);
    I.trim();

    if (t - I.last_hop >= I.cfg.hop_s - 1e-6) {
        I.last_hop = t;
        I.streaming_update(t, I.rec.size());
        info.new_vitals = true;
    }

    info.face = r.face;
    if (!lm.empty()) {
        info.landmarks = lm;
        float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
        for (auto& p : info.landmarks) {
            p.x *= f;
            p.y *= f;
            x0 = std::min(x0, p.x);
            y0 = std::min(y0, p.y);
            x1 = std::max(x1, p.x);
            y1 = std::max(y1, p.y);
        }
        info.face_box = {x0, y0, x1 - x0, y1 - y0};
        if (want_rois) {
            info.rois = I.sampler.polygons(lm);
            for (auto& poly : info.rois)
                for (auto& p : poly) {
                    p.x *= f;
                    p.y *= f;
                }
        }
    }
    for (std::size_t k = 0; k < I.nets.size(); ++k)
        if (I.nets[k] && r.nn_valid[k]) {
            info.neural_bvp = r.nn[k];
            break;
        }
    return info;
}

bool Engine::save_session(const std::string& path) const {
    const auto& I = *impl_;
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    const std::uint64_t n = I.rec.size(), sz = sizeof(engine::FrameRecord);
    f.write(kSessionMagic, 8);
    f.write(reinterpret_cast<const char*>(&sz), 8);
    f.write(reinterpret_cast<const char*>(&n), 8);
    f.write(reinterpret_cast<const char*>(&I.t_start), 8);
    f.write(reinterpret_cast<const char*>(I.rec.data()), static_cast<std::streamsize>(n * sz));
    return static_cast<bool>(f);
}

bool Engine::load_session(const std::string& path) {
    auto& I = *impl_;
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    char magic[8];
    std::uint64_t n = 0, sz = 0;
    double t0 = 0;
    f.read(magic, 8);
    f.read(reinterpret_cast<char*>(&sz), 8);
    f.read(reinterpret_cast<char*>(&n), 8);
    f.read(reinterpret_cast<char*>(&t0), 8);
    if (!f || std::memcmp(magic, kSessionMagic, 8) != 0 || sz != sizeof(engine::FrameRecord)) return false;
    std::vector<engine::FrameRecord> rec(n);
    f.read(reinterpret_cast<char*>(rec.data()), static_cast<std::streamsize>(n * sz));
    if (!f) return false;
    I.rec.swap(rec);
    I.t_start = t0;
    I.started = true;
    I.reset_analysis();
    return true;
}

std::vector<VitalSigns> Engine::replay_streaming() {
    auto& I = *impl_;
    std::vector<VitalSigns> out;
    I.reset_analysis();
    for (std::size_t i = 0; i < I.rec.size(); ++i) {
        const double t = I.rec[i].t;
        if (t - I.last_hop >= I.cfg.hop_s - 1e-6) {
            I.last_hop = t;
            I.streaming_update(t, i + 1);
            out.push_back(I.vit);
        }
    }
    return out;
}

SessionReport Engine::analyze_session() const {
    const auto& I = *impl_;
    SessionReport rep;
    if (I.rec.size() < 32) return rep;
    engine::PrismCache prism;  // PRISM selects its parameters on the whole session
    const engine::SignalSet s = I.analyzer.signals(I.rec, 0, I.rec.size(), &prism, true);
    rep.fs = s.fs;
    rep.t0 = s.t0 - I.t_start;
    const std::size_t W = static_cast<std::size_t>(I.cfg.window_s * s.fs);
    const std::size_t hop = std::max<std::size_t>(1, static_cast<std::size_t>(I.cfg.hop_s * s.fs));
    std::vector<std::vector<double>> obs;
    std::vector<double> q;
    dsp::Signal fused(s.n, 0.0), wsum(s.n, 0.0);
    for (std::size_t k1 = std::min(W, s.n); k1 <= s.n; k1 += hop) {
        const std::size_t k0 = k1 - std::min(W, k1);
        const engine::WindowResult w = I.analyzer.window(s, k0, k1);
        WindowEstimate e;
        e.t_end = rep.t0 + static_cast<double>(k1) / s.fs;
        if (!w.fused.empty()) {
            e.hr_raw_bpm = e.hr_bpm = w.hr_raw;
            e.sqi = w.sqi;
            e.snr_db = w.snr_db;
            e.confidence = w.agreement;
            e.candidates = w.cand;
            obs.push_back(w.fused);
            q.push_back(w.sqi);
            const std::size_t n = w.bvp.size();
            for (std::size_t i = 0; i < n; ++i) {
                const double h = 0.5 - 0.5 * std::cos(2.0 * M_PI * (i + 0.5) / n);
                fused[k0 + i] += h * w.bvp[i];
                wsum[k0 + i] += h;
            }
        } else {
            obs.push_back(std::vector<double>(I.analyzer.grid().size(), 1.0));
            q.push_back(0.0);
        }
        rep.windows.push_back(std::move(e));
        if (k1 == s.n) break;
    }
    if (I.cfg.track && !obs.empty()) {
        const auto& g = I.analyzer.grid();
        const auto path = engine::HrTracker::viterbi(g, obs, q, I.cfg.hop_s, I.cfg.track_sigma_bpm_per_s);
        const double step = g.size() > 1 ? g[1] - g[0] : 1.0;
        for (std::size_t i = 0; i < path.size(); ++i) {
            // Sub-grid refinement: local spectral maximum within +-2 bpm of the Viterbi state.
            const auto& o = obs[i];
            std::size_t k = static_cast<std::size_t>(std::lround((path[i] - g[0]) / step));
            const std::size_t lo = k >= 4 ? k - 4 : 0, hi = std::min(o.size() - 1, k + 4);
            for (std::size_t j = lo; j <= hi; ++j)
                if (o[j] > o[k]) k = j;
            double hr_ref = g[k];
            if (k > 0 && k + 1 < o.size()) {
                const double a = o[k - 1], b = o[k], c = o[k + 1], den = a - 2 * b + c;
                if (std::abs(den) > 1e-300) hr_ref += std::clamp(0.5 * (a - c) / den, -0.5, 0.5) * step;
            }
            rep.windows[i].hr_bpm = hr_ref;
        }
    }
    for (std::size_t i = 0; i < s.n; ++i) fused[i] = wsum[i] > 0 ? fused[i] / wsum[i] : 0.0;

    const double lo = I.cfg.hr_min_bpm / 60.0, hi = I.cfg.hr_max_bpm / 60.0;
    auto whole_hr = [&](const dsp::Signal& x) {
        if (x.size() < 16 || dsp::stdev(x) <= 0) return 0.0;
        const auto sp = dsp::periodogram(x, s.fs, std::max<std::size_t>(1 << 14, dsp::next_pow2(4 * x.size())));
        return dsp::spectral_peak(sp, lo, hi).freq * 60.0;
    };
    const double hr_fused = whole_hr(fused);
    for (std::size_t i = 0; i < s.names.size(); ++i) {
        dsp::Signal x = s.bvp[i];
        if (!s.parts[i].empty()) {
            // Whole-session multi-ROI combination with goodness weights at the session HR.
            x.assign(s.n, 0.0);
            double gs = 0;
            for (const auto& p : s.parts[i]) {
                const auto sp = dsp::periodogram(p, s.fs, dsp::next_pow2(4 * p.size()));
                const double snr = std::pow(10.0, dsp::snr_db(sp, hr_fused / 60.0, lo, hi) / 10.0);
                const double g = std::sqrt(snr * (1 + snr));
                for (std::size_t k = 0; k < s.n; ++k) x[k] += g * p[k];
                gs += g;
            }
            if (gs > 0)
                for (double& v : x) v /= gs;
        }
        rep.names.push_back(s.names[i]);
        rep.bvp.emplace_back(x.begin(), x.end());
        rep.whole_hr_bpm.push_back(whole_hr(x));
    }
    rep.names.push_back("FUSED");
    rep.bvp.emplace_back(fused.begin(), fused.end());
    rep.whole_hr_bpm.push_back(hr_fused);
    rep.hrv = engine::hrv_from_bvp(fused, s.fs, hr_fused);
    double conf = 0;
    rep.rr_bpm = engine::respiration_rate(s, 0, s.n, fused, hr_fused, conf);
    return rep;
}

}  // namespace rppg
