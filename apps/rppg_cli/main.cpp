// rppg_cli: offline processing of a facial video.
//
//   rppg_cli --video face.mp4 [--models models] [--out result.json] [--bvp-csv bvp.csv]
//            [--session cache.rppgsess] [--methods POS,PRISM] [--neural facephys] [--quiet]
#include <chrono>
#include <cstdio>
#include <fstream>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include "app_util.hpp"

namespace {
void dump_overlay(const cv::Mat& frame, const rppg::FrameInfo& fi, const std::string& path) {
    cv::Mat v = frame.clone();
    for (const auto& poly : fi.rois) {
        std::vector<cv::Point> pts;
        for (const auto& p : poly) pts.emplace_back(static_cast<int>(p.x), static_cast<int>(p.y));
        cv::polylines(v, pts, true, {255, 180, 0}, 1, cv::LINE_AA);
    }
    for (const auto& p : fi.landmarks) cv::circle(v, {static_cast<int>(p.x), static_cast<int>(p.y)}, 1, {0, 255, 0}, -1);
    cv::imwrite(path, v);
}
}  // namespace

int main(int argc, char** argv) {
    app::Args args(argc, argv);
    const std::string video = args.str("video", args.positional.empty() ? "" : args.positional[0]);
    if (video.empty()) {
        std::fprintf(stderr,
                     "usage: rppg_cli --video FILE [--models DIR] [--out JSON] [--bvp-csv CSV] [--session FILE]\n"
                     "                [--methods POS,PRISM,...] [--neural facephys,me] [--window 10] [--quiet]\n");
        return 2;
    }
    rppg::EngineConfig cfg;
    app::apply_config(args, cfg);
    cfg.keep_all_frames = true;
    const bool quiet = args.has("quiet");

    cv::VideoCapture cap(video);
    if (!cap.isOpened()) {
        std::fprintf(stderr, "cannot open %s\n", video.c_str());
        return 1;
    }
    double fps = cap.get(cv::CAP_PROP_FPS);
    if (args.has("fps")) fps = args.num("fps", 30.0);
    if (!(fps > 1 && fps < 1000)) fps = 30.0;

    std::unique_ptr<rppg::Engine> engine;
    try {
        engine = std::make_unique<rppg::Engine>(cfg);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "engine init failed: %s\n", e.what());
        return 1;
    }
    cv::Mat frame;
    std::size_t n = 0, faces = 0;
    const auto t0 = std::chrono::steady_clock::now();
    const double dump_every = args.num("dump-every", 0.0);  // seconds between overlay snapshots
    const std::string dump_dir = args.str("dump-dir", ".");
    const double max_seconds = args.num("max-seconds", 1e18);
    double next_dump = 0.0;
    while (cap.read(frame)) {
        const double ts = static_cast<double>(n) / fps;
        if (ts > max_seconds) break;
        const bool dump = dump_every > 0 && ts >= next_dump;
        const rppg::FrameInfo fi = engine->process(app::view_of(frame), ts, nullptr, dump);
        if (dump) {
            dump_overlay(frame, fi, dump_dir + "/frame_" + std::to_string(static_cast<int>(ts)) + "s.png");
            next_dump += dump_every;
        }
        faces += fi.face;
        ++n;
        if (fi.new_vitals && !quiet) {
            const auto v = engine->vitals();
            if (v.valid)
                std::printf("t=%7.2fs  HR %6.1f bpm (raw %6.1f)  SQI %.2f  SNR %5.1f dB  conf %.2f  RR %4.1f\n",
                            v.timestamp, v.hr_bpm, v.hr_raw_bpm, v.sqi, v.snr_db, v.confidence, v.rr_bpm);
        }
    }
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("processed %zu frames (%.1f s of video) in %.1f s: %.1f fps, face in %.1f%% of frames\n", n,
                n / fps, secs, n / std::max(secs, 1e-9), 100.0 * faces / std::max<std::size_t>(n, 1));

    if (args.has("session")) engine->save_session(args.str("session"));
    const rppg::SessionReport rep = engine->analyze_session();
    std::printf("whole-session HR:");
    for (std::size_t i = 0; i < rep.names.size(); ++i) std::printf("  %s=%.1f", rep.names[i].c_str(), rep.whole_hr_bpm[i]);
    std::printf("\n");
    if (rep.hrv.valid)
        std::printf("HRV: beats=%d meanHR=%.1f SDNN=%.1f ms RMSSD=%.1f ms pNN50=%.2f LF/HF=%.2f\n", rep.hrv.beats,
                    rep.hrv.mean_hr_bpm, rep.hrv.sdnn_ms, rep.hrv.rmssd_ms, rep.hrv.pnn50, rep.hrv.lf_hf);
    std::printf("respiration: %.1f breaths/min\n", rep.rr_bpm);

    if (args.has("bvp-csv") && !rep.bvp.empty()) {
        std::ofstream f(args.str("bvp-csv"));
        f << "t";
        for (const auto& nm : rep.names) f << ',' << nm;
        f << '\n';
        for (std::size_t k = 0; k < rep.bvp.back().size(); ++k) {
            f << rep.t0 + k / rep.fs;
            for (const auto& b : rep.bvp) f << ',' << (k < b.size() ? b[k] : 0.f);
            f << '\n';
        }
    }
    if (args.has("out")) {
        std::ofstream f(args.str("out"));
        f << "{\n  \"video\": \"" << app::json_escape(video) << "\",\n  \"frames\": " << n << ",\n  \"fps\": " << fps
          << ",\n  \"whole_hr\": {";
        for (std::size_t i = 0; i < rep.names.size(); ++i)
            f << (i ? ", " : "") << '"' << rep.names[i] << "\": " << rep.whole_hr_bpm[i];
        f << "},\n  \"windows\": [\n";
        for (std::size_t w = 0; w < rep.windows.size(); ++w) {
            const auto& e = rep.windows[w];
            f << "    {\"t\": " << e.t_end << ", \"hr\": " << e.hr_bpm << ", \"hr_raw\": " << e.hr_raw_bpm
              << ", \"sqi\": " << e.sqi << ", \"snr\": " << e.snr_db << ", \"cand\": {";
            for (std::size_t c = 0; c < e.candidates.size(); ++c)
                f << (c ? ", " : "") << '"' << e.candidates[c].name << "\": [" << e.candidates[c].hr_bpm << ", "
                  << e.candidates[c].snr_db << ", " << e.candidates[c].weight << "]";
            f << "}}" << (w + 1 < rep.windows.size() ? "," : "") << "\n";
        }
        f << "  ],\n  \"hrv\": {\"valid\": " << (rep.hrv.valid ? "true" : "false") << ", \"sdnn\": " << rep.hrv.sdnn_ms
          << ", \"rmssd\": " << rep.hrv.rmssd_ms << ", \"pnn50\": " << rep.hrv.pnn50 << ", \"lf_hf\": " << rep.hrv.lf_hf
          << "},\n  \"rr\": " << rep.rr_bpm << "\n}\n";
    }
    return 0;
}
