// Signal-processing primitives used by the rPPG engine. Dependency free, double precision.
#pragma once

#include <array>
#include <complex>
#include <cstddef>
#include <vector>

namespace rppg::dsp {

using Signal = std::vector<double>;
using cpx = std::complex<double>;

// ----------------------------------------------------------------------------------------------
// FFT
// ----------------------------------------------------------------------------------------------
std::size_t next_pow2(std::size_t n);
// In-place DFT of arbitrary length (radix-2 for powers of two, Bluestein otherwise).
void fft(std::vector<cpx>& a, bool inverse = false);
// One-sided |X_k|^2, k = 0..nfft/2, of real x (zero padded or truncated to nfft).
Signal power_spectrum(const Signal& x, std::size_t nfft);

// ----------------------------------------------------------------------------------------------
// Butterworth IIR filters as cascaded second-order sections (scipy.signal.butter(output='sos')).
// ----------------------------------------------------------------------------------------------
struct Biquad {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;  // a0 == 1
};
using Sos = std::vector<Biquad>;
enum class BandType { LowPass, HighPass, BandPass };

// order = prototype order (band-pass filters have 2*order poles, like scipy).
Sos butter(int order, BandType type, double f1_hz, double f2_hz, double fs);
Signal sosfilt(const Sos& sos, const Signal& x);
// Zero-phase forward-backward filtering with odd padding and steady-state initial conditions
// (numerically equivalent to scipy.signal.sosfiltfilt with default arguments).
Signal sosfiltfilt(const Sos& sos, const Signal& x);
Signal bandpass(const Signal& x, double fs, double lo_hz, double hi_hz, int order = 2);

// Causal streaming filter (Direct Form II transposed).
class SosFilter {
public:
    SosFilter() = default;
    explicit SosFilter(Sos sos) : sos_(std::move(sos)), z_(sos_.size()) {}
    double step(double x);
    void reset() { for (auto& z : z_) z = {0.0, 0.0}; }
    bool empty() const { return sos_.empty(); }

private:
    Sos sos_;
    std::vector<std::array<double, 2>> z_;
};

// ----------------------------------------------------------------------------------------------
// Smoothing / detrending
// ----------------------------------------------------------------------------------------------
// Solves (I + mu * D2' D2) z = x with D2 the second-difference operator (Whittaker smoother,
// Hodrick-Prescott filter). O(N) banded LDL^T.
Signal whittaker_smooth(const Signal& x, double mu);
// Smoothness-priors detrending of Tarvainen et al. (IEEE TBME 2002): x - smooth(x, lambda^2).
Signal detrend_tarvainen(const Signal& x, double lambda);
// Cubic smoothing spline baseline (penalty lambda * int f''^2, lambda in s^3) for uniform samples.
Signal smoothing_spline(const Signal& x, double lambda_s3, double fs);
Signal remove_mean(const Signal& x);
Signal zscore(const Signal& x);
Signal moving_average(const Signal& x, std::size_t width);  // centered, edge-shrinking

// ----------------------------------------------------------------------------------------------
// Spectral analysis
// ----------------------------------------------------------------------------------------------
enum class Window { Rect, Hann, Hamming };
Signal make_window(Window w, std::size_t n);

struct Spectrum {
    Signal freq;   // Hz
    Signal power;  // power spectral density (arbitrary but consistent units)
    double df() const { return freq.size() > 1 ? freq[1] - freq[0] : 0.0; }
};

Spectrum periodogram(const Signal& x, double fs, std::size_t nfft, Window w = Window::Hann);
Spectrum welch(const Signal& x, double fs, std::size_t nperseg, std::size_t noverlap, std::size_t nfft,
               Window w = Window::Hann);

struct Peak {
    double freq = 0.0;
    double power = 0.0;
    std::size_t index = 0;
};
// Highest bin in [lo, hi] refined by parabolic interpolation.
Peak spectral_peak(const Spectrum& s, double lo, double hi);
// All local maxima in [lo, hi], strongest first (at most max_peaks).
std::vector<Peak> spectral_peaks(const Spectrum& s, double lo, double hi, std::size_t max_peaks);
double band_power(const Spectrum& s, double lo, double hi);
double spectrum_at(const Spectrum& s, double f);  // linear interpolation
// SNR in dB (de Haan & Jeanne 2013; rPPG-Toolbox): power within +-dev of f0 and 2*f0 over the
// remaining power inside [lo, hi].
double snr_db(const Spectrum& s, double f0, double lo, double hi, double dev = 0.1);
// Lomb-Scargle periodogram for unevenly sampled data (normalized power).
Spectrum lomb_scargle(const Signal& t, const Signal& y, const Signal& freqs);

// ----------------------------------------------------------------------------------------------
// Interpolation / resampling
// ----------------------------------------------------------------------------------------------
enum class Interp { Linear, Cubic };
// Natural cubic spline through (t, y); t strictly increasing.
class CubicSpline {
public:
    CubicSpline() = default;
    CubicSpline(const Signal& t, const Signal& y);
    double operator()(double x) const;
    bool empty() const { return t_.empty(); }

private:
    Signal t_, y_, m_;  // m_ = second derivatives
};
// Samples (t, y) on t0 + k / fs, k = 0..n-1. Values outside [t.front(), t.back()] are clamped.
Signal resample_uniform(const Signal& t, const Signal& y, double t0, double fs, std::size_t n,
                        Interp method = Interp::Cubic);

// ----------------------------------------------------------------------------------------------
// Peaks (time domain)
// ----------------------------------------------------------------------------------------------
// Local maxima separated by at least min_distance samples (taller peaks win) and with
// topographic prominence >= min_prominence (scipy.signal.find_peaks semantics).
std::vector<std::size_t> find_peaks(const Signal& x, std::size_t min_distance, double min_prominence = 0.0);
double peak_prominence(const Signal& x, std::size_t i);
// Sub-sample location of a local extremum via parabola through (i-1, i, i+1).
double refine_peak(const Signal& x, std::size_t i);

// ----------------------------------------------------------------------------------------------
// Statistics
// ----------------------------------------------------------------------------------------------
double mean(const Signal& x);
double variance(const Signal& x, int ddof = 0);
double stdev(const Signal& x, int ddof = 0);
double median(Signal x);
double mad(const Signal& x);  // median absolute deviation (unscaled)
double percentile(Signal x, double p);  // p in [0, 100], linear interpolation
double skewness(const Signal& x);
double kurtosis(const Signal& x);  // excess kurtosis
double pearson(const Signal& a, const Signal& b);
// Normalized autocorrelation r[k], k = 0..max_lag (biased estimator, r[0] = 1).
Signal autocorr(const Signal& x, std::size_t max_lag);

// ----------------------------------------------------------------------------------------------
// Small dense linear algebra (3x3 problems of the colour-space methods)
// ----------------------------------------------------------------------------------------------
using Mat3 = std::array<std::array<double, 3>, 3>;
using Vec3 = std::array<double, 3>;
// Symmetric eigen-decomposition (cyclic Jacobi). Eigenvalues descending; V columns = vectors.
void eig_sym3(const Mat3& A, Vec3& evals, Mat3& V);
bool solve3(const Mat3& A, const Vec3& b, Vec3& x);

}  // namespace rppg::dsp
