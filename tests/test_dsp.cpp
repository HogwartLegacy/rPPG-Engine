#include <fstream>
#include <map>
#include <random>
#include <sstream>

#include "rppg/dsp.hpp"
#include "rppg/methods.hpp"
#include "test_main.hpp"

using namespace rppg;
using dsp::Signal;

namespace {

std::map<std::string, Signal>& ref() {
    static std::map<std::string, Signal> data = [] {
        std::map<std::string, Signal> d;
        std::ifstream f(RPPG_TEST_DATA_DIR "/dsp_ref.txt");
        std::string line;
        while (std::getline(f, line)) {
            std::istringstream ss(line);
            std::string name;
            std::size_t n = 0;
            ss >> name >> n;
            Signal v(n);
            for (auto& x : v) ss >> x;
            d[name] = v;
        }
        return d;
    }();
    return data;
}

double max_abs_diff(const Signal& a, const Signal& b) {
    if (a.size() != b.size()) return 1e30;
    double m = 0;
    for (std::size_t i = 0; i < a.size(); ++i) m = std::max(m, std::abs(a[i] - b[i]));
    return m;
}

double max_abs(const Signal& a) {
    double m = 0;
    for (double v : a) m = std::max(m, std::abs(v));
    return m;
}

}  // namespace

TEST_CASE("reference data present") { CHECK(ref().count("x") == 1 && ref()["x"].size() == 300); }

TEST_CASE("fft matches naive DFT (pow2 and Bluestein)") {
    for (std::size_t n : {16u, 15u, 37u}) {
        std::vector<dsp::cpx> a(n);
        std::mt19937 rng(1);
        std::normal_distribution<double> nd;
        for (auto& v : a) v = {nd(rng), nd(rng)};
        auto b = a;
        dsp::fft(b);
        double err = 0;
        for (std::size_t k = 0; k < n; ++k) {
            dsp::cpx acc = 0;
            for (std::size_t j = 0; j < n; ++j) acc += a[j] * std::polar(1.0, -2.0 * M_PI * double(k * j) / double(n));
            err = std::max(err, std::abs(acc - b[k]));
        }
        CHECK(err < 1e-9);
        dsp::fft(b, true);
        double rt_err = 0;
        for (std::size_t k = 0; k < n; ++k) rt_err = std::max(rt_err, std::abs(a[k] - b[k]));
        CHECK(rt_err < 1e-12);
    }
}

TEST_CASE("butterworth + sosfiltfilt match scipy") {
    const Signal& x = ref()["x"];
    auto bp2 = dsp::butter(2, dsp::BandType::BandPass, 0.7, 3.0, 30.0);
    CHECK(bp2.size() == 2);
    CHECK(max_abs_diff(dsp::sosfiltfilt(bp2, x), ref()["sosfiltfilt_bp2"]) < 1e-8);
    auto bp3 = dsp::butter(3, dsp::BandType::BandPass, 0.65, 3.5, 30.0);
    CHECK(max_abs_diff(dsp::sosfiltfilt(bp3, x), ref()["sosfiltfilt_bp3"]) < 1e-8);
    auto lp4 = dsp::butter(4, dsp::BandType::LowPass, 2.0, 0.0, 30.0);
    CHECK(max_abs_diff(dsp::sosfiltfilt(lp4, x), ref()["sosfiltfilt_lp4"]) < 1e-8);
    auto hp3 = dsp::butter(3, dsp::BandType::HighPass, 0.5, 0.0, 30.0);
    CHECK(max_abs_diff(dsp::sosfilt(hp3, x), ref()["sosfilt_hp3"]) < 1e-8);
}

TEST_CASE("smoothness-priors detrend matches dense solve") {
    const Signal& x = ref()["x"];
    CHECK(max_abs_diff(dsp::detrend_tarvainen(x, 100.0), ref()["detrend100"]) < 1e-7);
    CHECK(max_abs_diff(dsp::detrend_tarvainen(x, 10.0), ref()["detrend10"]) < 1e-8);
}

TEST_CASE("periodogram and welch match scipy") {
    const Signal& x = ref()["x"];
    const auto p = dsp::periodogram(x, 30.0, 1024, dsp::Window::Hann);
    const Signal& rp = ref()["periodogram_hann_1024"];
    CHECK(max_abs_diff(p.power, rp) < 1e-9 * std::max(1.0, max_abs(rp)));
    const auto w = dsp::welch(x, 30.0, 128, 64, 512, dsp::Window::Hann);
    const Signal& rw = ref()["welch_hann_128_64_512"];
    CHECK(max_abs_diff(w.power, rw) < 1e-9 * std::max(1.0, max_abs(rw)));
    const auto pk = dsp::spectral_peak(p, 0.7, 3.0);
    CHECK_NEAR(pk.freq, 1.2, 0.03);
}

TEST_CASE("find_peaks matches scipy") {
    const Signal& x = ref()["x"];
    auto cmp = [&](const std::vector<std::size_t>& a, const Signal& b) {
        if (a.size() != b.size()) return false;
        for (std::size_t i = 0; i < a.size(); ++i)
            if (double(a[i]) != b[i]) return false;
        return true;
    };
    CHECK(cmp(dsp::find_peaks(x, 10), ref()["find_peaks_d10"]));
    CHECK(cmp(dsp::find_peaks(x, 5, 0.8), ref()["find_peaks_d5_p08"]));
}

TEST_CASE("cubic spline resampling reproduces smooth signal") {
    Signal t, y;
    std::mt19937 rng(3);
    std::uniform_real_distribution<double> jitter(-0.004, 0.004);
    for (int i = 0; i < 600; ++i) {
        const double ti = i / 29.97 + jitter(rng);
        t.push_back(ti);
        y.push_back(std::sin(2 * M_PI * 1.3 * ti));
    }
    const Signal u = dsp::resample_uniform(t, y, 1.0, 30.0, 450, dsp::Interp::Cubic);
    double err = 0;
    for (std::size_t k = 0; k < u.size(); ++k) err = std::max(err, std::abs(u[k] - std::sin(2 * M_PI * 1.3 * (1.0 + k / 30.0))));
    CHECK(err < 5e-3);
}

TEST_CASE("eig_sym3 decomposes symmetric matrix") {
    dsp::Mat3 A = {{{4, 1, 0.5}, {1, 3, 0.2}, {0.5, 0.2, 1}}};
    dsp::Vec3 ev;
    dsp::Mat3 V;
    dsp::eig_sym3(A, ev, V);
    CHECK(ev[0] >= ev[1] && ev[1] >= ev[2]);
    for (int k = 0; k < 3; ++k)
        for (int r = 0; r < 3; ++r) {
            double av = 0;
            for (int c = 0; c < 3; ++c) av += A[r][c] * V[c][k];
            CHECK_NEAR(av, ev[k] * V[r][k], 1e-10);
        }
}

// Synthetic skin reflectance model: DC skin colour * (1 + pulse * pulsatility + common intensity
// variation) + per-channel specular / noise (Wang et al. 2017 skin optics model).
static RgbTrace synthetic_trace(double hr_hz, double fs, double seconds, unsigned seed, double noise = 0.002) {
    RgbTrace t;
    t.fs = fs;
    std::mt19937 rng(seed);
    std::normal_distribution<double> nd;
    const double dc[3] = {180, 120, 95};
    const double puls[3] = {0.33, 0.77, 0.53};  // normalised pulsatile signature
    const std::size_t n = static_cast<std::size_t>(seconds * fs);
    for (std::size_t i = 0; i < n; ++i) {
        const double tt = i / fs;
        const double p = 0.004 * (std::sin(2 * M_PI * hr_hz * tt) + 0.3 * std::sin(4 * M_PI * hr_hz * tt + 0.7));
        const double inten = 0.03 * std::sin(2 * M_PI * 0.15 * tt) + 0.01 * std::sin(2 * M_PI * 0.9 * tt + 1.0);
        const double spec = 0.004 * std::sin(2 * M_PI * 0.45 * tt);
        double v[3];
        for (int c = 0; c < 3; ++c) v[c] = dc[c] * (1 + inten + puls[c] * p) + 255 * spec + dc[c] * noise * nd(rng);
        t.r.push_back(v[0]);
        t.g.push_back(v[1]);
        t.b.push_back(v[2]);
        dsp::Mat3 C{};
        for (int a = 0; a < 3; ++a)
            for (int b = 0; b < 3; ++b) C[a][b] = v[a] * v[b] + (a == b ? 4.0 : 0.0) + (a != b ? 1.0 : 0.0);
        t.corr.push_back(C);
    }
    return t;
}

TEST_CASE("all classical methods recover synthetic pulse rate") {
    const double hr = 1.25;  // 75 bpm
    const RgbTrace t = synthetic_trace(hr, 30.0, 30.0, 11);
    for (Method m : all_methods()) {
        if (m == Method::SSR) continue;  // needs real pixel distributions
        const Signal bvp = dsp::bandpass(extract_bvp(m, t), 30.0, 0.65, 3.5);
        const auto sp = dsp::welch(bvp, 30.0, 256, 192, 2048);
        const double est = dsp::spectral_peak(sp, 0.65, 3.5).freq;
        // GREEN / ICA / PCA have no illumination invariance: the in-band 0.9 Hz intensity
        // flicker of this trace is expected to defeat them (reported, not asserted).
        const bool invariant = m != Method::Green && m != Method::ICA && m != Method::PCA;
        if (std::abs(est - hr) > 0.05) std::printf("  method %s -> %.3f Hz%s\n", method_name(m), est, invariant ? "" : " (expected)");
        if (invariant) CHECK_NEAR(est, hr, 0.05);
    }
}

TEST_CASE("PRISM selects parameters and recovers pulse") {
    const RgbTrace t = synthetic_trace(1.6, 30.0, 60.0, 5);
    const PrismChoice c = prism_select(t);
    CHECK_NEAR(c.mean_hr_bpm, 96.0, 2.0);
    CHECK(c.alpha >= 0.5 && c.alpha <= 1.0);
}

int main() { return rt::run_all(); }
