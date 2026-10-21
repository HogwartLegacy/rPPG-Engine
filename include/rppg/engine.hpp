// rPPG engine: facial video -> blood volume pulse, heart rate, signal quality, HRV, respiration.
//
//   rppg::EngineConfig cfg;            // defaults = best configuration found in docs/BENCHMARK.md
//   cfg.model_dir = "models";
//   rppg::Engine engine(cfg);
//   for (each camera frame) {
//       rppg::FrameInfo fi = engine.process(view, timestamp_seconds);
//       if (fi.new_vitals) { auto v = engine.vitals(); if (v.valid) show(v.hr_bpm, v.sqi); }
//   }
//   rppg::SessionReport r = engine.analyze_session();   // optional offline analysis
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "rppg/methods.hpp"
#include "rppg/types.hpp"

namespace rppg {

enum class NeuralModel { None, FacePhys, MeRppg };

struct EngineConfig {
    std::string model_dir = "models";
    int threads = 1;  // ONNX Runtime intra-op threads per model

    // --- signal sources ---------------------------------------------------------------------
    // Unsupervised colour methods evaluated on the facial ROIs. The "-MR" candidates fuse forehead
    // and cheek regions with SNR (maximal-ratio) weights; plain candidates use the holistic region.
    // Defaults = best all-round configuration of docs/BENCHMARK.md (UBFC-rPPG + MCD-rPPG). For
    // low-end phones drop MeRppg (one network per frame); classical-only also works without ONNX.
    std::vector<Method> methods{Method::POS};
    bool multi_roi = true;     // add a multi-region POS candidate (DistancePPG style weighting)
    std::vector<NeuralModel> neural{NeuralModel::FacePhys, NeuralModel::MeRppg};  // streaming deep models
    bool use_face_mesh = true;  // false: caller must pass landmarks to process()

    // --- analysis -------------------------------------------------------------------------------
    double window_s = 10.0;    // spectral analysis window
    double hop_s = 1.0;        // vitals update period
    double fs = 30.0;          // internal uniform sampling rate
    double hr_min_bpm = 40.0;
    double hr_max_bpm = 200.0;
    bool fuse = true;          // quality-weighted spectral fusion of all candidates
    double classical_prior = 0.5;  // reliability prior of colour-method candidates vs neural ones
    // Colour methods degrade on turned faces (foreshortened / self-occluded skin): their prior is
    // multiplied by 1 / (1 + (a / pose_asym_ref)^4), a = |log| left/right cheek skin-pixel ratio.
    bool pose_aware_prior = true;
    double pose_asym_ref = 0.5;
    // Same for head motion (colour traces carry motion artefacts, the networks are trained with
    // moving subjects): prior / (1 + (motion / motion_prior_ref)^2), motion in face widths / s.
    double motion_prior_ref = 0.0;  // <= 0 disables (no benefit measured, kept for experiments)
    double fusion_power = 1.0;     // fusion weight = prior * SNR^power
    bool track = true;         // HMM heart-rate tracking across windows
    double track_sigma_bpm_per_s = 4.0;  // expected HR drift
    // Motion-aware spectral attenuation (pyVHR-style). Off by default: with face-locked ROIs and HMM
    // tracking it removes pulse energy when HR coincides with head motion (BENCHMARK.md ablation).
    bool motion_compensation = false;
    int max_side = 0;          // downscale frames so max(w, h) <= max_side (0 = full resolution)
    double max_history_s = 600.0;  // streaming memory (older frames are dropped)
    bool keep_all_frames = false;  // offline mode: keep the whole session for analyze_session()

    // --- face tracking ------------------------------------------------------------------------
    double smooth_min_cutoff = 1.0;
    double smooth_beta = 2.0;
    int redetect_interval = 0;

    // false: no model is loaded (analysis of saved sessions only; process() is unavailable).
    bool load_models = true;
};

struct HrvMetrics {
    bool valid = false;
    int beats = 0;
    double mean_hr_bpm = 0, mean_ibi_ms = 0, sdnn_ms = 0, rmssd_ms = 0, pnn50 = 0;
    double lf = 0, hf = 0, lf_hf = 0;
};

struct CandidateEstimate {
    std::string name;
    double hr_bpm = 0;
    double snr_db = -30;
    double weight = 0;
};

struct VitalSigns {
    double timestamp = 0;     // end of the analysis window (s)
    bool valid = false;       // window long enough and quality acceptable
    double hr_bpm = 0;        // tracked heart rate
    double hr_raw_bpm = 0;    // spectral peak of the fused spectrum (untracked)
    double confidence = 0;    // posterior mass around the estimate [0, 1]
    double sqi = 0;           // signal quality index [0, 1]
    double snr_db = -30;
    double rr_bpm = 0;        // respiration rate (breaths / min), 0 if unavailable
    double rr_confidence = 0;
    HrvMetrics hrv;           // from the last 60 s when quality allows
    std::vector<CandidateEstimate> candidates;
    std::vector<float> bvp;   // fused pulse waveform of the analysis window (z-scored)
    double bvp_fs = 0;
    double face_ratio = 0;    // fraction of frames with a tracked face in the window
    double motion = 0;        // head motion level (face widths / s)
    bool low_light = false, saturation = false;
};

struct FrameInfo {
    bool face = false;
    RectF face_box;
    std::vector<Point2f> landmarks;            // smoothed 478-point mesh (image coordinates)
    std::vector<std::vector<Point2f>> rois;    // ROI polygons (only if requested)
    float neural_bvp = 0.f;                    // latest raw output of the first neural model
    bool new_vitals = false;
};

struct WindowEstimate {
    double t_end = 0;                 // window end time (s, session clock)
    double hr_bpm = 0;                // tracked (Viterbi) estimate
    double hr_raw_bpm = 0;
    double sqi = 0, snr_db = -30, confidence = 0;
    std::vector<CandidateEstimate> candidates;
};

struct SessionReport {
    double fs = 0;
    double t0 = 0;                                // session time of sample 0
    std::vector<std::string> names;               // candidate names (+ "FUSED")
    std::vector<std::vector<float>> bvp;          // full-length pulse per candidate
    std::vector<double> whole_hr_bpm;             // whole-session spectral HR per candidate
    std::vector<WindowEstimate> windows;          // sliding windows (window_s, hop_s)
    HrvMetrics hrv;
    double rr_bpm = 0;
};

class Engine {
public:
    explicit Engine(const EngineConfig& cfg);
    ~Engine();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    // Processes one frame. timestamp: seconds on a monotonic clock (video PTS or capture time).
    // external_landmarks: optional 468/478-point MediaPipe mesh in image coordinates (used when
    // use_face_mesh == false, e.g. landmarks from ARKit / ML Kit).
    FrameInfo process(const ImageView& frame, double timestamp, const std::vector<Point2f>* external_landmarks = nullptr,
                      bool want_rois = false);
    VitalSigns vitals() const;
    SessionReport analyze_session() const;
    void reset();

    // Research / benchmarking: the per-frame traces (ROI colours, pose, neural outputs) can be saved
    // and re-analysed with any analysis configuration without decoding the video again.
    bool save_session(const std::string& path) const;
    bool load_session(const std::string& path);
    // Re-runs the causal (real-time) analysis over the loaded session: one VitalSigns per hop.
    std::vector<VitalSigns> replay_streaming();
    std::size_t frames() const;
    const EngineConfig& config() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

const char* version();

// HRV of any pulse waveform (rPPG or contact PPG) with the engine's beat detector and IBI cleaning.
HrvMetrics hrv_from_pulse(const std::vector<double>& pulse, double fs, double hr_hint_bpm);

}  // namespace rppg
