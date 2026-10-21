// rppg_live: real-time heart rate from a webcam (or any OpenCV capture source).
//
//   rppg_live [--camera 0 | --video file] [--models models] [--width 640 --height 480]
//   keys: q / ESC quit, r ROIs, m mesh, c candidates, s save session (live_session.rppgsess)
#include <chrono>
#include <cstdio>
#include <deque>

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include "app_util.hpp"

namespace {

cv::Scalar quality_color(double sqi) {
    if (sqi >= 0.6) return {80, 200, 80};
    if (sqi >= 0.3) return {0, 190, 255};
    return {60, 60, 230};
}

void draw_wave(cv::Mat& img, const std::deque<float>& w, cv::Rect area, cv::Scalar color) {
    cv::rectangle(img, area, {30, 30, 30}, cv::FILLED);
    if (w.size() < 2) return;
    float lo = 1e9f, hi = -1e9f;
    for (float v : w) {
        lo = std::min(lo, v);
        hi = std::max(hi, v);
    }
    const float span = std::max(hi - lo, 1e-6f);
    std::vector<cv::Point> pts;
    for (std::size_t i = 0; i < w.size(); ++i) {
        const int x = area.x + static_cast<int>(i * (area.width - 1) / (w.size() - 1));
        const int y = area.y + area.height - 1 - static_cast<int>((w[i] - lo) / span * (area.height - 8)) - 4;
        pts.emplace_back(x, y);
    }
    cv::polylines(img, pts, false, color, 2, cv::LINE_AA);
}

}  // namespace

int main(int argc, char** argv) {
    app::Args args(argc, argv);
    rppg::EngineConfig cfg;
    app::apply_config(args, cfg);
    cv::VideoCapture cap;
    if (args.has("video"))
        cap.open(args.str("video"));
    else
        cap.open(static_cast<int>(args.num("camera", 0)));
    if (!cap.isOpened()) {
        std::fprintf(stderr, "cannot open the camera / video\n");
        return 1;
    }
    cap.set(cv::CAP_PROP_FRAME_WIDTH, args.num("width", 640));
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, args.num("height", 480));
    cap.set(cv::CAP_PROP_FPS, args.num("fps", 30));

    std::unique_ptr<rppg::Engine> engine;
    try {
        engine = std::make_unique<rppg::Engine>(cfg);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "engine init failed: %s\n", e.what());
        return 1;
    }
    bool show_roi = true, show_mesh = false, show_cand = true;
    const auto clock0 = std::chrono::steady_clock::now();
    std::deque<float> raw_wave;
    rppg::VitalSigns v;
    cv::Mat frame;
    double fps_est = 0;
    auto last = clock0;
    const bool from_file = args.has("video");
    std::size_t idx = 0;
    const double file_fps = from_file ? std::max(1.0, cap.get(cv::CAP_PROP_FPS)) : 30.0;
    while (cap.read(frame)) {
        const auto now = std::chrono::steady_clock::now();
        const double t = from_file ? static_cast<double>(idx++) / file_fps : std::chrono::duration<double>(now - clock0).count();
        const double dt = std::chrono::duration<double>(now - last).count();
        last = now;
        if (dt > 0) fps_est = fps_est > 0 ? 0.9 * fps_est + 0.1 / dt : 1.0 / dt;

        const rppg::FrameInfo fi = engine->process(app::view_of(frame), t, nullptr, show_roi);
        if (fi.new_vitals) v = engine->vitals();
        if (fi.face) {
            raw_wave.push_back(fi.neural_bvp);
            if (raw_wave.size() > 300) raw_wave.pop_front();
        }

        cv::Mat view = frame.clone();
        if (fi.face) {
            cv::rectangle(view, cv::Rect(static_cast<int>(fi.face_box.x), static_cast<int>(fi.face_box.y),
                                         static_cast<int>(fi.face_box.w), static_cast<int>(fi.face_box.h)),
                          {200, 200, 200}, 1);
            if (show_mesh)
                for (const auto& p : fi.landmarks) cv::circle(view, {static_cast<int>(p.x), static_cast<int>(p.y)}, 1, {0, 255, 0}, -1);
            if (show_roi)
                for (const auto& poly : fi.rois) {
                    std::vector<cv::Point> pts;
                    for (const auto& p : poly) pts.emplace_back(static_cast<int>(p.x), static_cast<int>(p.y));
                    cv::polylines(view, pts, true, {255, 180, 0}, 1, cv::LINE_AA);
                }
        } else {
            cv::putText(view, "no face", {20, 60}, cv::FONT_HERSHEY_SIMPLEX, 1.0, {60, 60, 230}, 2);
        }
        const int W = view.cols;
        cv::Mat panel(170, W, CV_8UC3, cv::Scalar(20, 20, 20));
        char buf[256];
        const cv::Scalar qc = quality_color(v.sqi);
        if (v.valid)
            std::snprintf(buf, sizeof(buf), "%5.1f bpm", v.hr_bpm);
        else
            std::snprintf(buf, sizeof(buf), "  --  bpm");
        cv::putText(panel, buf, {12, 50}, cv::FONT_HERSHEY_SIMPLEX, 1.5, qc, 3, cv::LINE_AA);
        std::snprintf(buf, sizeof(buf), "SQI %.2f  SNR %.1f dB  conf %.2f", v.sqi, v.snr_db, v.confidence);
        cv::putText(panel, buf, {12, 80}, cv::FONT_HERSHEY_SIMPLEX, 0.5, {220, 220, 220}, 1, cv::LINE_AA);
        std::snprintf(buf, sizeof(buf), "RR %.1f /min   %s  %.0f fps", v.rr_bpm,
                      v.hrv.valid ? (std::string("SDNN ") + std::to_string(static_cast<int>(v.hrv.sdnn_ms)) + " ms  RMSSD " +
                                     std::to_string(static_cast<int>(v.hrv.rmssd_ms)) + " ms").c_str()
                                  : "HRV: collecting",
                      fps_est);
        cv::putText(panel, buf, {12, 102}, cv::FONT_HERSHEY_SIMPLEX, 0.5, {220, 220, 220}, 1, cv::LINE_AA);
        if (v.motion > 0.6) cv::putText(panel, "hold still", {W - 130, 30}, cv::FONT_HERSHEY_SIMPLEX, 0.6, {0, 190, 255}, 2);
        if (v.low_light) cv::putText(panel, "low light", {W - 130, 55}, cv::FONT_HERSHEY_SIMPLEX, 0.6, {0, 190, 255}, 2);
        if (show_cand) {
            int y = 20;
            for (const auto& c : v.candidates) {
                std::snprintf(buf, sizeof(buf), "%-9s %5.1f  w %.2f", c.name.c_str(), c.hr_bpm, c.weight);
                cv::putText(panel, buf, {W / 2 - 20, y}, cv::FONT_HERSHEY_PLAIN, 1.0, {180, 180, 180}, 1);
                y += 15;
            }
        }
        std::deque<float> fused(v.bvp.begin(), v.bvp.end());
        draw_wave(panel, fused, {12, 112, W / 2 - 24, 52}, qc);
        draw_wave(panel, raw_wave, {W / 2, 112, W / 2 - 12, 52}, {200, 120, 255});
        cv::Mat out;
        cv::vconcat(view, panel, out);
        cv::imshow("rPPG", out);
        const int key = cv::waitKey(1) & 0xFF;
        if (key == 'q' || key == 27) break;
        if (key == 'r') show_roi = !show_roi;
        if (key == 'm') show_mesh = !show_mesh;
        if (key == 'c') show_cand = !show_cand;
        if (key == 's') engine->save_session("live_session.rppgsess");
    }
    return 0;
}
