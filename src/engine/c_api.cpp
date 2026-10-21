#include "rppg/c_api.h"

#include <algorithm>
#include <cstring>
#include <exception>
#include <string>

#include "rppg/engine.hpp"

struct rppg_engine {
    std::unique_ptr<rppg::Engine> engine;
    rppg::FrameInfo last;
    std::string error;
};

namespace {
void copy_err(char* dst, int len, const std::string& msg) {
    if (!dst || len <= 0) return;
    const std::size_t n = std::min<std::size_t>(static_cast<std::size_t>(len - 1), msg.size());
    std::memcpy(dst, msg.data(), n);
    dst[n] = '\0';
}
}  // namespace

extern "C" {

const char* rppg_version(void) { return rppg::version(); }

void rppg_default_config(rppg_config* c) {
    if (!c) return;
    const rppg::EngineConfig d;
    c->model_dir = "models";
    c->threads = d.threads;
    c->neural_model = RPPG_NEURAL_BOTH;
    c->classical = 1;
    c->window_s = d.window_s;
    c->hop_s = d.hop_s;
    c->hr_min_bpm = d.hr_min_bpm;
    c->hr_max_bpm = d.hr_max_bpm;
    c->max_side = d.max_side;
}

rppg_engine* rppg_create(const rppg_config* c, char* err, int err_len) {
    try {
        rppg::EngineConfig cfg;
        rppg_config def;
        if (!c) {
            rppg_default_config(&def);
            c = &def;
        }
        if (c->model_dir) cfg.model_dir = c->model_dir;
        cfg.threads = c->threads > 0 ? c->threads : 1;
        cfg.neural.clear();
        if (c->neural_model == RPPG_NEURAL_FACEPHYS || c->neural_model == RPPG_NEURAL_BOTH)
            cfg.neural.push_back(rppg::NeuralModel::FacePhys);
        if (c->neural_model == RPPG_NEURAL_ME_RPPG || c->neural_model == RPPG_NEURAL_BOTH)
            cfg.neural.push_back(rppg::NeuralModel::MeRppg);
        if (!c->classical) {
            cfg.methods.clear();
            cfg.multi_roi = false;
        }
        if (c->window_s > 0) cfg.window_s = c->window_s;
        if (c->hop_s > 0) cfg.hop_s = c->hop_s;
        if (c->hr_min_bpm > 0) cfg.hr_min_bpm = c->hr_min_bpm;
        if (c->hr_max_bpm > cfg.hr_min_bpm) cfg.hr_max_bpm = c->hr_max_bpm;
        cfg.max_side = c->max_side;
        auto* e = new rppg_engine;
        e->engine = std::make_unique<rppg::Engine>(cfg);
        return e;
    } catch (const std::exception& ex) {
        copy_err(err, err_len, ex.what());
        return nullptr;
    }
}

void rppg_destroy(rppg_engine* e) { delete e; }

void rppg_reset(rppg_engine* e) {
    if (e) e->engine->reset();
}

int rppg_process(rppg_engine* e, const uint8_t* data, int width, int height, int stride, int format, const uint8_t* uv,
                 int uv_stride, int rotation, double ts, rppg_frame_result* out) {
    if (!e) return -1;
    try {
        rppg::ImageView v;
        v.data = data;
        v.width = width;
        v.height = height;
        v.stride = stride;
        v.format = static_cast<rppg::PixelFormat>(format);
        v.uv = uv;
        v.uv_stride = uv_stride;
        v.rotation = rotation;
        e->last = e->engine->process(v, ts);
        if (out) {
            out->face = e->last.face ? 1 : 0;
            out->x = e->last.face_box.x;
            out->y = e->last.face_box.y;
            out->w = e->last.face_box.w;
            out->h = e->last.face_box.h;
            out->neural_bvp = e->last.neural_bvp;
            out->new_vitals = e->last.new_vitals ? 1 : 0;
        }
        return 0;
    } catch (const std::exception& ex) {
        e->error = ex.what();
        return -2;
    }
}

int rppg_get_vitals(rppg_engine* e, rppg_vitals* o) {
    if (!e || !o) return -1;
    const rppg::VitalSigns v = e->engine->vitals();
    std::memset(o, 0, sizeof(*o));
    o->timestamp = v.timestamp;
    o->valid = v.valid ? 1 : 0;
    o->hr_bpm = v.hr_bpm;
    o->hr_raw_bpm = v.hr_raw_bpm;
    o->confidence = v.confidence;
    o->sqi = v.sqi;
    o->snr_db = v.snr_db;
    o->rr_bpm = v.rr_bpm;
    o->rr_confidence = v.rr_confidence;
    o->hrv_valid = v.hrv.valid ? 1 : 0;
    o->hrv_mean_hr_bpm = v.hrv.mean_hr_bpm;
    o->sdnn_ms = v.hrv.sdnn_ms;
    o->rmssd_ms = v.hrv.rmssd_ms;
    o->pnn50 = v.hrv.pnn50;
    o->lf_hf = v.hrv.lf_hf;
    o->face_ratio = v.face_ratio;
    o->motion = v.motion;
    o->low_light = v.low_light ? 1 : 0;
    o->saturation = v.saturation ? 1 : 0;
    return 0;
}

int rppg_get_bvp(rppg_engine* e, float* buf, int capacity, double* fs) {
    if (!e) return 0;
    const rppg::VitalSigns v = e->engine->vitals();
    if (fs) *fs = v.bvp_fs;
    const int n = std::min(capacity, static_cast<int>(v.bvp.size()));
    if (buf && n > 0) std::memcpy(buf, v.bvp.data() + (v.bvp.size() - static_cast<std::size_t>(n)), sizeof(float) * n);
    return n;
}

int rppg_get_landmarks(rppg_engine* e, float* xy, int cap) {
    if (!e || !xy) return 0;
    const int n = std::min(cap, static_cast<int>(e->last.landmarks.size()));
    for (int i = 0; i < n; ++i) {
        xy[2 * i] = e->last.landmarks[static_cast<std::size_t>(i)].x;
        xy[2 * i + 1] = e->last.landmarks[static_cast<std::size_t>(i)].y;
    }
    return n;
}

const char* rppg_last_error(rppg_engine* e) { return e ? e->error.c_str() : "null engine"; }

}  // extern "C"
