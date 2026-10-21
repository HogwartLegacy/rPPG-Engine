/* C interface of the rPPG engine for FFI (Android JNI, Swift / Objective-C, Flutter, React Native). */
#ifndef RPPG_C_API_H
#define RPPG_C_API_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) && defined(RPPG_SHARED)
#ifdef RPPG_BUILDING_DLL
#define RPPG_API __declspec(dllexport)
#else
#define RPPG_API __declspec(dllimport)
#endif
#elif defined(__GNUC__)
#define RPPG_API __attribute__((visibility("default")))
#else
#define RPPG_API
#endif

typedef struct rppg_engine rppg_engine;

/* Pixel formats (same values as rppg::PixelFormat). */
enum { RPPG_RGB8 = 0, RPPG_BGR8 = 1, RPPG_RGBA8 = 2, RPPG_BGRA8 = 3, RPPG_NV21 = 4, RPPG_NV12 = 5 };
/* Neural models. */
enum { RPPG_NEURAL_NONE = 0, RPPG_NEURAL_FACEPHYS = 1, RPPG_NEURAL_ME_RPPG = 2, RPPG_NEURAL_BOTH = 3 };

typedef struct rppg_config {
    const char* model_dir;
    int threads;
    int neural_model;  /* RPPG_NEURAL_* (default BOTH; FACEPHYS for low-end phones) */
    int classical;     /* 1: POS + multi-ROI POS candidates, 0: neural only */
    double window_s;
    double hop_s;
    double hr_min_bpm;
    double hr_max_bpm;
    int max_side;      /* downscale frames (0 = full resolution) */
} rppg_config;

typedef struct rppg_frame_result {
    int face;
    float x, y, w, h;  /* face box (pixels) */
    float neural_bvp;
    int new_vitals;
} rppg_frame_result;

typedef struct rppg_vitals {
    double timestamp;
    int valid;
    double hr_bpm, hr_raw_bpm, confidence, sqi, snr_db;
    double rr_bpm, rr_confidence;
    int hrv_valid;
    double hrv_mean_hr_bpm, sdnn_ms, rmssd_ms, pnn50, lf_hf;
    double face_ratio, motion;
    int low_light, saturation;
} rppg_vitals;

RPPG_API const char* rppg_version(void);
RPPG_API void rppg_default_config(rppg_config* cfg);
/* Returns NULL on failure (message copied into err when err != NULL). */
RPPG_API rppg_engine* rppg_create(const rppg_config* cfg, char* err, int err_len);
RPPG_API void rppg_destroy(rppg_engine* e);
RPPG_API void rppg_reset(rppg_engine* e);
/* Returns 0 on success. uv / uv_stride only for NV12 / NV21. rotation: 0, 90, 180, 270 (clockwise). */
RPPG_API int rppg_process(rppg_engine* e, const uint8_t* data, int width, int height, int stride, int format,
                          const uint8_t* uv, int uv_stride, int rotation, double timestamp_s,
                          rppg_frame_result* out);
RPPG_API int rppg_get_vitals(rppg_engine* e, rppg_vitals* out);
/* Copies the latest fused pulse waveform; returns the number of samples written. */
RPPG_API int rppg_get_bvp(rppg_engine* e, float* buf, int capacity, double* fs);
/* Copies the last frame's landmarks as x0, y0, x1, y1, ...; returns the number of points. */
RPPG_API int rppg_get_landmarks(rppg_engine* e, float* xy, int capacity_points);
RPPG_API const char* rppg_last_error(rppg_engine* e);

#ifdef __cplusplus
}
#endif

#endif /* RPPG_C_API_H */
