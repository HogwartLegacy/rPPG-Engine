// rppg_bench: benchmark against reference contact-PPG of public datasets.
//
// 1) capture: run the vision front-end (face mesh, ROI sampling, neural models) once per video and
//    cache the per-frame traces together with the frame-aligned reference PPG.
//      rppg_bench capture --dataset ubfc|mcd|folder --root DIR --cache DIR [--jobs 8] [--neural facephys,me]
// 2) eval: re-analyse every cached session with an analysis configuration; writes one CSV row per
//    (video, protocol, window, candidate) with predicted and reference heart rate.
//      rppg_bench eval --cache DIR --out results.csv --name CONFIG [engine options...]
//
// Protocols: "whole"  = one HR per video (periodogram of the whole pulse signal, rPPG-Toolbox),
//            "window" = sliding windows (window_s, hop_s) of the offline analysis (Viterbi tracked),
//            "stream" = causal real-time output of the engine (what an app displays).
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>

#include "app_util.hpp"
#include "augment.hpp"
#include "rppg/dsp.hpp"

namespace fs = std::filesystem;
using rppg::dsp::Signal;

namespace {

struct VideoItem {
    std::string id, video, gt;
    std::string dataset;
};

std::vector<VideoItem> list_ubfc(const std::string& root) {
    std::vector<VideoItem> v;
    for (const auto& d : fs::directory_iterator(root)) {
        if (!d.is_directory()) continue;
        const fs::path vid = d.path() / "vid.avi", gt = d.path() / "ground_truth.txt";
        if (fs::exists(vid) && fs::exists(gt) && !fs::exists(d.path() / "vid.avi.aria2"))
            v.push_back({d.path().filename().string(), vid.string(), gt.string(), "ubfc"});
    }
    std::sort(v.begin(), v.end(), [](const VideoItem& a, const VideoItem& b) {
        auto num = [](const std::string& s) { return std::atoi(s.c_str() + std::min<std::size_t>(7, s.size())); };
        return num(a.id) < num(b.id);
    });
    return v;
}

std::vector<VideoItem> list_mcd(const std::string& root) {
    std::vector<VideoItem> v;
    for (const auto& f : fs::directory_iterator(fs::path(root) / "video")) {
        if (f.path().extension() != ".avi") continue;
        if (fs::exists(f.path().string() + ".aria2")) continue;  // still downloading
        const std::string id = f.path().stem().string();
        const fs::path gt = fs::path(root) / "ppg_sync" / (id + ".txt");
        if (fs::exists(gt)) v.push_back({id, f.path().string(), gt.string(), "mcd"});
    }
    std::sort(v.begin(), v.end(), [](const VideoItem& a, const VideoItem& b) { return a.id < b.id; });
    return v;
}

// Reference PPG, one value per video frame.
bool load_gt(const VideoItem& it, Signal& ppg) {
    std::ifstream f(it.gt);
    if (!f) return false;
    ppg.clear();
    if (it.dataset == "ubfc") {
        std::string line;
        std::getline(f, line);  // line 1: PPG waveform (line 2: HR, line 3: time)
        std::istringstream ss(line);
        double v;
        while (ss >> v) ppg.push_back(v);
    } else {
        std::string line;
        while (std::getline(f, line)) {
            std::istringstream ss(line);
            double v;
            if (ss >> v) ppg.push_back(v);
        }
    }
    return ppg.size() > 16;
}

bool write_floats(const std::string& path, const Signal& t, const Signal& y) {
    std::ofstream f(path, std::ios::binary);
    const std::uint64_t n = t.size();
    f.write(reinterpret_cast<const char*>(&n), 8);
    for (std::size_t i = 0; i < n; ++i) {
        const double a[2] = {t[i], y[i]};
        f.write(reinterpret_cast<const char*>(a), 16);
    }
    return static_cast<bool>(f);
}

bool read_floats(const std::string& path, Signal& t, Signal& y) {
    std::ifstream f(path, std::ios::binary);
    std::uint64_t n = 0;
    f.read(reinterpret_cast<char*>(&n), 8);
    if (!f || n > (1ull << 28)) return false;
    t.resize(n);
    y.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        double a[2];
        f.read(reinterpret_cast<char*>(a), 16);
        t[i] = a[0];
        y[i] = a[1];
    }
    return static_cast<bool>(f);
}

int capture(const app::Args& args) {
    const std::string dataset = args.str("dataset");
    const std::string root = args.str("root");
    const std::string cache = args.str("cache");
    const int jobs = std::max(1, static_cast<int>(args.num("jobs", 4)));
    std::vector<VideoItem> items = dataset == "ubfc" ? list_ubfc(root) : list_mcd(root);
    if (args.has("filter")) {
        const auto keys = app::split(args.str("filter"));
        items.erase(std::remove_if(items.begin(), items.end(),
                                   [&](const VideoItem& it) {
                                       for (const auto& k : keys)
                                           if (it.id.find(k) != std::string::npos) return false;
                                       return true;
                                   }),
                    items.end());
    }
    if (args.has("limit")) items.resize(std::min<std::size_t>(items.size(), static_cast<std::size_t>(args.num("limit", 1e9))));
    // Optional re-encoded copies (e.g. H.264 CRF sweeps): <video-dir>/<id>.mp4 replaces the original.
    if (args.has("video-dir"))
        for (auto& it : items) {
            const fs::path alt = fs::path(args.str("video-dir")) / (it.id + ".mp4");
            if (fs::exists(alt)) it.video = alt.string();
        }
    const std::string aug_spec = args.str("augment", "none");
    const double max_seconds = args.num("max-seconds", 1e9);
    fs::create_directories(cache);
    rppg::EngineConfig cfg;
    cfg.methods.clear();  // capture only needs the front-end; analysis happens in eval
    cfg.multi_roi = false;
    cfg.neural = {rppg::NeuralModel::FacePhys, rppg::NeuralModel::MeRppg};
    app::apply_config(args, cfg);
    cfg.keep_all_frames = true;
    cfg.hop_s = 1e9;  // no streaming analysis during capture
    std::printf("capture %zu videos of %s with %d jobs\n", items.size(), dataset.c_str(), jobs);
    std::atomic<std::size_t> next{0};
    std::mutex io;
    auto worker_body = [&]() {
        std::unique_ptr<rppg::Engine> eng;
        for (;;) {
            const std::size_t k = next++;
            if (k >= items.size()) return;
            const VideoItem& it = items[k];
            const std::string base = (fs::path(cache) / it.id).string();
            if (fs::exists(base + ".rppgsess") && !args.has("force")) continue;
            Signal ppg;
            if (!load_gt(it, ppg)) {
                std::lock_guard<std::mutex> l(io);
                std::printf("  [%s] cannot read ground truth\n", it.id.c_str());
                continue;
            }
            if (!eng) eng = std::make_unique<rppg::Engine>(cfg);
            eng->reset();
            cv::VideoCapture cap(it.video);
            double fps = cap.get(cv::CAP_PROP_FPS);
            if (!(fps > 1 && fps < 1000)) fps = 30.0;
            cv::Mat frame;
            std::size_t n = 0, faces = 0;
            bench::Augment aug(aug_spec);
            aug.begin(static_cast<unsigned>(std::hash<std::string>{}(it.id)));
            const auto t0 = std::chrono::steady_clock::now();
            while (cap.read(frame)) {
                const double ts = static_cast<double>(n) / fps;
                if (ts > max_seconds) break;
                aug.apply(frame, ts);
                faces += eng->process(app::view_of(frame), ts).face;
                ++n;
            }
            const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            Signal t(std::min(n, ppg.size())), y(t.size());
            for (std::size_t i = 0; i < t.size(); ++i) {
                t[i] = static_cast<double>(i) / fps;
                y[i] = ppg[i];
            }
            eng->save_session(base + ".rppgsess.tmp");
            write_floats(base + ".gt", t, y);
            // Freshly written files can be briefly locked (e.g. by an anti-virus scan): retry.
            bool moved = false;
            for (int attempt = 0; attempt < 50 && !moved; ++attempt) {
                std::error_code ec;
                fs::rename(base + ".rppgsess.tmp", base + ".rppgsess", ec);
                moved = !ec;
                if (!moved) std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
            if (!moved) {
                std::error_code ec;
                fs::copy_file(base + ".rppgsess.tmp", base + ".rppgsess", fs::copy_options::overwrite_existing, ec);
                fs::remove(base + ".rppgsess.tmp", ec);
            }
            std::ofstream(base + ".info") << "fps " << fps << "\nframes " << n << "\nfaces " << faces << "\nppg " << ppg.size()
                                          << "\nseconds " << secs << "\n";
            std::lock_guard<std::mutex> l(io);
            std::printf("  [%3zu/%zu] %-32s %5zu frames  %5.1f fps  face %5.1f%%  gt %zu\n", k + 1, items.size(),
                        it.id.c_str(), n, n / std::max(secs, 1e-9), 100.0 * faces / std::max<std::size_t>(1, n), ppg.size());
            std::fflush(stdout);
        }
    };
    auto worker = [&]() {
        try {
            worker_body();
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> l(io);
            std::fprintf(stderr, "capture worker failed: %s\n", e.what());
        }
    };
    std::vector<std::thread> pool;
    for (int j = 0; j < jobs; ++j) pool.emplace_back(worker);
    for (auto& th : pool) th.join();
    return 0;
}

// Reference HR of a PPG segment with the same spectral estimator as the predictions.
struct GtHr {
    double fs = 30.0;
    Signal x;  // conditioned reference on a uniform grid starting at t = 0
    double lo, hi;
    GtHr(const Signal& t, const Signal& y, double fs_, double lo_bpm, double hi_bpm) : fs(fs_) {
        lo = lo_bpm / 60.0;
        hi = hi_bpm / 60.0;
        const std::size_t n = static_cast<std::size_t>(t.back() * fs) + 1;
        Signal u = rppg::dsp::resample_uniform(t, y, 0.0, fs, n, rppg::dsp::Interp::Linear);
        u = rppg::dsp::detrend_tarvainen(u, 100.0 * fs / 30.0);
        x = rppg::dsp::zscore(rppg::dsp::bandpass(u, fs, lo, hi, 2));
    }
    double hr(double t0, double t1) const {
        const std::size_t k0 = static_cast<std::size_t>(std::max(0.0, t0 * fs));
        const std::size_t k1 = std::min(x.size(), static_cast<std::size_t>(std::max(0.0, t1 * fs)));
        if (k1 <= k0 + 16) return 0.0;
        const Signal seg(x.begin() + static_cast<std::ptrdiff_t>(k0), x.begin() + static_cast<std::ptrdiff_t>(k1));
        const auto sp = rppg::dsp::periodogram(seg, fs, std::max<std::size_t>(1 << 14, rppg::dsp::next_pow2(4 * seg.size())));
        return rppg::dsp::spectral_peak(sp, lo, hi).freq * 60.0;
    }
};

int eval(const app::Args& args) {
    const std::string cache = args.str("cache");
    const std::string name = args.str("name", "default");
    const bool no_stream = args.has("no-stream");
    rppg::EngineConfig cfg;
    app::apply_config(args, cfg);
    cfg.load_models = false;
    cfg.keep_all_frames = true;
    std::vector<std::string> sessions;
    for (const auto& f : fs::directory_iterator(cache))
        if (f.path().extension() == ".rppgsess") sessions.push_back(f.path().string());
    std::sort(sessions.begin(), sessions.end());
    if (args.has("filter")) {
        const std::string flt = args.str("filter");
        sessions.erase(std::remove_if(sessions.begin(), sessions.end(),
                                      [&](const std::string& s) { return s.find(flt) == std::string::npos; }),
                       sessions.end());
    }
    const std::string dataset = args.str("dataset", fs::path(cache).filename().string());
    const int jobs = std::max(1, static_cast<int>(args.num("jobs", 8)));
    std::vector<std::string> rows(sessions.size());
    std::atomic<std::size_t> next{0};
    auto worker = [&]() {
        rppg::Engine eng(cfg);
        for (;;) {
            const std::size_t k = next++;
            if (k >= sessions.size()) return;
            const fs::path p(sessions[k]);
            const std::string id = p.stem().string();
            Signal gt_t, gt_y;
            if (!eng.load_session(sessions[k]) ||
                !read_floats((p.parent_path() / (id + ".gt")).string(), gt_t, gt_y) || gt_t.size() < 64)
                continue;
            const GtHr gt(gt_t, gt_y, cfg.fs, cfg.hr_min_bpm, cfg.hr_max_bpm);
            std::ostringstream o;
            o.precision(3);
            o << std::fixed;
            auto row = [&](const char* proto, double t, const std::string& cand, double pred, double ref, double sqi,
                           double snr, double conf) {
                o << dataset << ',' << id << ',' << name << ',' << proto << ',' << t << ',' << cand << ',' << pred << ','
                  << ref << ',' << sqi << ',' << snr << ',' << conf << '\n';
            };
            const rppg::SessionReport rep = eng.analyze_session();
            const double dur = gt_t.back();
            const double ref_whole = gt.hr(0.0, dur);
            // Pulse-rate variability: rPPG fused pulse vs. the contact PPG, same beat detector.
            const rppg::HrvMetrics ref_hrv = rppg::hrv_from_pulse(gt.x, cfg.fs, ref_whole);
            auto hrv_rows = [&](const std::string& src, const rppg::HrvMetrics& h) {
                if (!h.valid || !ref_hrv.valid) return;
                row("hrv", dur, "meanHR@" + src, h.mean_hr_bpm, ref_hrv.mean_hr_bpm, 0, 0, 0);
                row("hrv", dur, "SDNN@" + src, h.sdnn_ms, ref_hrv.sdnn_ms, 0, 0, 0);
                row("hrv", dur, "RMSSD@" + src, h.rmssd_ms, ref_hrv.rmssd_ms, 0, 0, 0);
                row("hrv", dur, "pNN50@" + src, 100 * h.pnn50 + 1e-3, 100 * ref_hrv.pnn50 + 1e-3, 0, 0, 0);
            };
            hrv_rows("engine", rep.hrv);
            for (std::size_t c = 0; c < rep.names.size(); ++c) {
                const Signal x(rep.bvp[c].begin(), rep.bvp[c].end());
                hrv_rows(rep.names[c], rppg::hrv_from_pulse(x, rep.fs, rep.whole_hr_bpm[c]));
            }
            for (std::size_t c = 0; c < rep.names.size(); ++c) row("whole", dur, rep.names[c], rep.whole_hr_bpm[c], ref_whole, 0, 0, 0);
            for (const auto& w : rep.windows) {
                const double ref = gt.hr(w.t_end - cfg.window_s, w.t_end);
                if (ref <= 0) continue;
                row("window", w.t_end, "FUSED", w.hr_bpm, ref, w.sqi, w.snr_db, w.confidence);
                row("window", w.t_end, "FUSED_RAW", w.hr_raw_bpm, ref, w.sqi, w.snr_db, w.confidence);
                for (const auto& c : w.candidates) row("window", w.t_end, c.name, c.hr_bpm, ref, w.sqi, c.snr_db, c.weight);
            }
            if (!no_stream) {
                const auto st = eng.replay_streaming();
                for (const auto& v : st) {
                    if (v.hr_bpm <= 0 || v.timestamp < cfg.window_s) continue;
                    const double ref = gt.hr(v.timestamp - cfg.window_s, v.timestamp);
                    if (ref <= 0) continue;
                    row("stream", v.timestamp, "FUSED", v.hr_bpm, ref, v.sqi, v.snr_db, v.confidence);
                }
            }
            rows[k] = o.str();
        }
    };
    std::vector<std::thread> pool;
    for (int j = 0; j < jobs; ++j) pool.emplace_back(worker);
    for (auto& th : pool) th.join();
    const std::string out = args.str("out", "results.csv");
    const bool exists = fs::exists(out);
    std::ofstream f(out, std::ios::app);
    if (!exists) f << "dataset,video,config,protocol,t,candidate,hr_pred,hr_gt,sqi,snr,conf\n";
    std::size_t n = 0;
    for (const auto& r : rows) {
        f << r;
        n += !r.empty();
    }
    std::printf("eval %s: %zu sessions -> %s\n", name.c_str(), n, out.c_str());
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    app::Args args(argc, argv);
    const std::string cmd = args.positional.empty() ? "" : args.positional[0];
    try {
        if (cmd == "capture") return capture(args);
        if (cmd == "eval") return eval(args);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "rppg_bench: %s\n", e.what());
        return 1;
    }
    std::fprintf(stderr, "usage: rppg_bench capture|eval [options]   (see source header)\n");
    return 2;
}
