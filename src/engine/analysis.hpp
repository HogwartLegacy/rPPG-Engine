// Internal signal analysis shared by the streaming engine and the offline session report.
#pragma once

#include <array>
#include <string>
#include <vector>

#include "rppg/dsp.hpp"
#include "rppg/engine.hpp"
#include "vision/roi.hpp"

namespace rppg::engine {

constexpr int kMaxNeural = 2;
// Fixed record slots of the neural models, so saved sessions can be re-analysed with any subset.
inline int neural_slot(NeuralModel m) { return m == NeuralModel::FacePhys ? 0 : (m == NeuralModel::MeRppg ? 1 : -1); }

// Everything the analysis needs from one video frame (about 0.5 kB).
struct FrameRecord {
    double t = 0;
    bool face = false;
    std::array<std::array<float, 3>, vision::kNumRegions> rgb{};  // mean skin RGB per region
    std::array<std::uint32_t, vision::kNumRegions> n{};          // skin pixels (0 = invalid)
    std::array<float, 6> corr{};      // holistic raw 2nd moments / n: RR GG BB RG RB GB (2SR)
    std::array<float, 3> bg{};
    bool bg_valid = false;
    float cx = 0, cy = 0, scale = 0, roll = 0, yaw = 0, pitch = 0;
    float luma = 0, saturated = 0;
    std::array<float, kMaxNeural> nn{};
    std::array<bool, kMaxNeural> nn_valid{};
};

// Uniformly resampled candidate pulse signals over a span of frames.
struct SignalSet {
    double t0 = 0, fs = 30.0;
    std::size_t n = 0;
    std::vector<std::string> names;
    std::size_t n_neural = 0;                     // neural candidates are the last n_neural names
    std::vector<dsp::Signal> bvp;                 // empty = candidate unavailable
    std::vector<std::vector<dsp::Signal>> parts;  // multi-ROI sub-signals (combined per window)
    dsp::Signal face;                             // 1 where a face was tracked
    std::array<dsp::Signal, 4> motion;            // centre x, centre y (face widths), log scale, roll
    dsp::Signal luma;                             // holistic green (respiration, low light)
    dsp::Signal asym;                             // |log| left / right cheek skin pixels (yaw proxy)
    dsp::Signal bg;                               // background green (illumination reference)
    double mean_luma = 0, saturated = 0;
};

struct PrismCache {
    bool ready = false;
    PrismChoice choice;
    double selected_at = -1e9;
};

struct WindowResult {
    bool valid = false;
    double hr_raw = 0, snr_db = -30, sqi = 0, agreement = 0, face_ratio = 0, motion = 0, asymmetry = 0;
    std::vector<double> fused;  // fused normalised spectrum on the HR grid
    std::vector<CandidateEstimate> cand;
    dsp::Signal bvp;            // fused window waveform
};

class Analyzer {
public:
    explicit Analyzer(const EngineConfig& cfg);
    const std::vector<double>& grid() const { return grid_; }  // bpm
    // Stage A: resample the records [i0, i1) and compute every candidate's pulse signal.
    SignalSet signals(const std::vector<FrameRecord>& rec, std::size_t i0, std::size_t i1, PrismCache* prism,
                      bool reselect_prism) const;
    // Stage B: spectral analysis / fusion of samples [k0, k1) of a signal set.
    WindowResult window(const SignalSet& s, std::size_t k0, std::size_t k1) const;
    // Re-runs the PRISM parameter search on the holistic trace of records [i0, i1).
    void reselect_prism(const std::vector<FrameRecord>& rec, std::size_t i0, std::size_t i1, PrismCache& cache) const;
    std::vector<std::string> candidate_names() const;

private:
    EngineConfig cfg_;
    std::vector<double> grid_;
    double lo_hz_, hi_hz_;
};

// Heart-rate tracking on the analysis grid: online forward filtering and offline Viterbi.
class HrTracker {
public:
    HrTracker(const std::vector<double>& grid_bpm, double sigma_bpm_per_s, double jump_prob = 0.02);
    void reset();
    // obs: normalised spectrum; quality in [0, 1] tempers the observation.
    void update(const std::vector<double>& obs, double dt, double quality, double& hr, double& confidence);
    static std::vector<double> viterbi(const std::vector<double>& grid_bpm, const std::vector<std::vector<double>>& obs,
                                       const std::vector<double>& quality, double dt, double sigma_bpm_per_s,
                                       double jump_prob = 0.02);

private:
    std::vector<double> grid_, post_;
    double sigma_, jump_;
    bool init_ = false;
};

// Physiological measures from a pulse waveform.
HrvMetrics hrv_from_bvp(const dsp::Signal& bvp, double fs, double hr_hint_bpm);
double respiration_rate(const SignalSet& s, std::size_t k0, std::size_t k1, const dsp::Signal& bvp_window,
                        double hr_bpm, double& confidence);

}  // namespace rppg::engine
