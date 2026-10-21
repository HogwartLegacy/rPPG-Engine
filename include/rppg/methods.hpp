// Classical (unsupervised) rPPG colour-space algorithms operating on uniformly sampled RGB traces.
#pragma once

#include <string>
#include <vector>

#include "rppg/dsp.hpp"

namespace rppg {

// Uniformly sampled colour traces of one skin region.
struct RgbTrace {
    double fs = 30.0;
    dsp::Signal r, g, b;
    // Optional per-frame raw RGB correlation matrices of the region's pixels (needed by 2SR).
    std::vector<dsp::Mat3> corr;
    std::size_t size() const { return g.size(); }
    RgbTrace slice(std::size_t begin, std::size_t end) const;
};

enum class Method { Green, ICA, PCA, CHROM, POS, LGI, PBV, OMIT, SSR, PRISM };
const char* method_name(Method m);
bool method_from_name(const std::string& name, Method& out);
std::vector<Method> all_methods();

struct MethodParams {
    double window_s = 0.0;   // analysis window of the projection; 0 selects the method's default
    double hr_lo_hz = 0.65;  // pulse band used by band-limited steps (CHROM filter, ICA/PCA selection)
    double hr_hi_hz = 3.5;
};

// Pulse signal of the whole trace. Window based methods are overlap-added (Hann) so the output
// has the same length as the input. Output is zero mean, unit variance (not band-pass filtered).
dsp::Signal extract_bvp(Method m, const RgbTrace& rgb, const MethodParams& p = {});

// --- PRISM (Projection-based Robust Interpretable Signal Mixing, NeurIPS 2025) ---------------
struct PrismParams {
    std::vector<double> alphas{0.5, 0.6, 0.7, 0.8, 0.9, 1.0};
    std::vector<double> lambdas{0.01, 0.05, 0.1, 0.5, 1.0};  // smoothing-spline penalty [s^3]
    double window_s = 10.0;   // non-overlapping HR windows of the objective
    double k_tv = 1.0 / 3.0;  // weight of the HR temporal variation term
    double high_lo = 0.75, high_hi = 4.0;  // pulse bands of the two-band harmonic check [Hz]
    double low_lo = 0.5, low_hi = 3.0;
    double harmonic_tol = 0.12;  // relative tolerance of f_high ~= 2 f_low
    std::size_t nfft = 1 << 14;
};
struct PrismChoice {
    double lambda = 0.1;
    double alpha = 0.7;
    bool low_band = false;
    double objective = 0.0;
    double mean_hr_bpm = 0.0;
    double band_lo = 0.75, band_hi = 4.0;
};
PrismChoice prism_select(const RgbTrace& rgb, const PrismParams& p = {});
// s(t) = G^ - (alpha B^ + (1 - alpha) R^), X^ = X / spline_baseline(X; lambda).
dsp::Signal prism_signal(const RgbTrace& rgb, double lambda, double alpha);

// --- building blocks (exposed for testing / research) ------------------------------------------
// JADE ICA (Cardoso 1999) of a 3 x N matrix (rows = channels). Returns sources (3 x N).
std::vector<dsp::Signal> jade3(const std::vector<dsp::Signal>& X);

}  // namespace rppg
