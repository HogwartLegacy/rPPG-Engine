// End-to-end tests of the engine analysis path without neural models: a synthetic "face" patch with
// a pulsatile skin-colour modulation, landmarks supplied externally (as ARKit / ML Kit would).
#include <cmath>
#include <cstdio>
#include <fstream>
#include <random>

#include "rppg/c_api.h"
#include "rppg/engine.hpp"
#include "test_main.hpp"

using namespace rppg;

namespace {

struct Synth {
    int w = 320, h = 240;
    std::vector<std::uint8_t> bgr;
    std::vector<Point2f> lm;
    std::mt19937 rng{42};
    Synth() : bgr(static_cast<std::size_t>(320 * 240 * 3)) {
        std::uniform_real_distribution<float> ux(90.f, 230.f), uy(50.f, 190.f);
        for (int i = 0; i < 478; ++i) lm.push_back({ux(rng), uy(rng)});
    }
    // Skin rectangle whose colour follows the pulse signature [0.33, 0.77, 0.53] at hr_bpm.
    void frame(double t, double hr_bpm, double motion_px = 0.0) {
        std::normal_distribution<double> nd(0.0, 1.5);
        const double p = 0.006 * std::sin(2 * M_PI * hr_bpm / 60.0 * t);
        const double light = 1.0 + 0.02 * std::sin(2 * M_PI * 0.2 * t);
        const int dx = static_cast<int>(std::lround(motion_px * std::sin(2 * M_PI * 0.3 * t)));
        const double base[3] = {95, 125, 185};  // B, G, R of a light skin tone
        const double puls[3] = {0.53, 0.77, 0.33};
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                const bool skin = x >= 80 + dx && x < 240 + dx && y >= 40 && y < 200;
                std::uint8_t* px = &bgr[static_cast<std::size_t>((y * w + x) * 3)];
                for (int c = 0; c < 3; ++c) {
                    const double v = skin ? base[c] * light * (1 + puls[c] * p) + nd(rng) : 60.0;
                    px[c] = static_cast<std::uint8_t>(std::clamp(v, 0.0, 255.0));
                }
            }
    }
    ImageView view() const {
        ImageView v;
        v.data = bgr.data();
        v.width = w;
        v.height = h;
        v.format = PixelFormat::BGR8;
        return v;
    }
};

}  // namespace

TEST_CASE("engine recovers pulse rate from synthetic skin video (external landmarks)") {
    EngineConfig cfg;
    cfg.load_models = false;
    cfg.use_face_mesh = false;
    cfg.neural.clear();
    cfg.keep_all_frames = true;
    Engine eng(cfg);
    Synth s;
    const double hr = 72.0;
    std::vector<double> streamed;
    for (int i = 0; i < 30 * 30; ++i) {
        const double t = i / 30.0;
        s.frame(t, hr);
        const FrameInfo fi = eng.process(s.view(), t, &s.lm);
        CHECK(fi.face);
        if (fi.new_vitals && t > 12) streamed.push_back(eng.vitals().hr_bpm);
    }
    const VitalSigns v = eng.vitals();
    CHECK(v.valid);
    CHECK_NEAR(v.hr_bpm, hr, 2.0);
    CHECK(v.sqi > 0.5);
    double worst = 0;
    for (double x : streamed) worst = std::max(worst, std::abs(x - hr));
    CHECK(worst < 3.0);
    const SessionReport rep = eng.analyze_session();
    CHECK(!rep.names.empty());
    CHECK_NEAR(rep.whole_hr_bpm.back(), hr, 1.5);
    CHECK(!rep.windows.empty());
}

TEST_CASE("session save / load / replay reproduces the streaming output") {
    EngineConfig cfg;
    cfg.load_models = false;
    cfg.use_face_mesh = false;
    cfg.neural.clear();
    cfg.keep_all_frames = true;
    Engine eng(cfg);
    Synth s;
    for (int i = 0; i < 20 * 30; ++i) {
        s.frame(i / 30.0, 95.0, 6.0);
        eng.process(s.view(), i / 30.0, &s.lm);
    }
    const double live = eng.vitals().hr_bpm;
    const std::string path = "test_session.rppgsess";
    CHECK(eng.save_session(path));
    Engine eng2(cfg);
    CHECK(eng2.load_session(path));
    CHECK(eng2.frames() == eng.frames());
    const auto replay = eng2.replay_streaming();
    CHECK(!replay.empty());
    CHECK_NEAR(replay.back().hr_bpm, live, 1e-6);
    CHECK_NEAR(live, 95.0, 2.5);
    std::remove(path.c_str());
}

TEST_CASE("C API with the real models (face detector, face mesh, neural rPPG)") {
    rppg_config c;
    rppg_default_config(&c);
    c.model_dir = RPPG_MODEL_DIR;
    char err[256];
    rppg_engine* e = rppg_create(&c, err, sizeof(err));
    if (!e) {
        std::printf("  models unavailable (%s), skipped\n", err);
        return;
    }
    Synth s;
    rppg_frame_result r{};
    for (int i = 0; i < 5; ++i) {
        s.frame(i / 30.0, 70.0);
        CHECK(rppg_process(e, s.bgr.data(), s.w, s.h, 0, RPPG_BGR8, nullptr, 0, 0, i / 30.0, &r) == 0);
    }
    CHECK(r.face == 0);  // a rectangle is not a face
    rppg_vitals v{};
    CHECK(rppg_get_vitals(e, &v) == 0);
    CHECK(v.valid == 0);
    rppg_destroy(e);
}

int main() { return rt::run_all(); }
