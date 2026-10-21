#include <algorithm>
#include <cctype>
#include <cmath>

#include "rppg/methods.hpp"

namespace rppg {

using dsp::Mat3;
using dsp::Signal;
using dsp::Vec3;

RgbTrace RgbTrace::slice(std::size_t begin, std::size_t end) const {
    RgbTrace t;
    t.fs = fs;
    end = std::min(end, size());
    begin = std::min(begin, end);
    const auto lo = static_cast<std::ptrdiff_t>(begin), hi = static_cast<std::ptrdiff_t>(end);
    t.r.assign(r.begin() + lo, r.begin() + hi);
    t.g.assign(g.begin() + lo, g.begin() + hi);
    t.b.assign(b.begin() + lo, b.begin() + hi);
    if (corr.size() >= end) t.corr.assign(corr.begin() + lo, corr.begin() + hi);
    return t;
}

const char* method_name(Method m) {
    switch (m) {
        case Method::Green: return "GREEN";
        case Method::ICA: return "ICA";
        case Method::PCA: return "PCA";
        case Method::CHROM: return "CHROM";
        case Method::POS: return "POS";
        case Method::LGI: return "LGI";
        case Method::PBV: return "PBV";
        case Method::OMIT: return "OMIT";
        case Method::SSR: return "2SR";
        case Method::PRISM: return "PRISM";
    }
    return "?";
}

bool method_from_name(const std::string& name, Method& out) {
    std::string s;
    for (char c : name) s.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    if (s == "SSR") s = "2SR";
    for (Method m : all_methods()) {
        if (s == method_name(m)) {
            out = m;
            return true;
        }
    }
    return false;
}

std::vector<Method> all_methods() {
    return {Method::Green, Method::ICA, Method::PCA, Method::CHROM, Method::POS,
            Method::LGI,   Method::PBV, Method::OMIT, Method::SSR,  Method::PRISM};
}

namespace {

struct Window3 {
    Signal r, g, b;
    std::size_t n() const { return g.size(); }
};

Window3 take(const RgbTrace& t, std::size_t s, std::size_t n) {
    Window3 w;
    const auto b = static_cast<std::ptrdiff_t>(s), e = static_cast<std::ptrdiff_t>(s + n);
    w.r.assign(t.r.begin() + b, t.r.begin() + e);
    w.g.assign(t.g.begin() + b, t.g.begin() + e);
    w.b.assign(t.b.begin() + b, t.b.begin() + e);
    return w;
}

// Divides every channel by its temporal mean (AC/DC normalisation).
void normalize(Window3& w) {
    for (Signal* c : {&w.r, &w.g, &w.b}) {
        const double m = dsp::mean(*c);
        const double inv = std::abs(m) > 1e-12 ? 1.0 / m : 0.0;
        for (double& v : *c) v *= inv;
    }
}

Signal finite_zscore(Signal x) {
    for (double& v : x)
        if (!std::isfinite(v)) v = 0.0;
    return dsp::zscore(x);
}

// Fraction of in-band power held by the strongest in-band spectral peak (periodicity score).
double periodicity(const Signal& x, double fs, double lo, double hi) {
    const auto sp = dsp::periodogram(x, fs, dsp::next_pow2(4 * x.size()), dsp::Window::Hann);
    const double band = dsp::band_power(sp, lo, hi);
    if (band <= 0.0) return 0.0;
    const dsp::Peak pk = dsp::spectral_peak(sp, lo, hi);
    const double w = 0.1;  // +-6 bpm around the peak
    return dsp::band_power(sp, pk.freq - w, pk.freq + w) / band;
}

// Generic Hann-weighted overlap-add of a per-window projection.
template <class F>
Signal window_ola(const RgbTrace& t, std::size_t win, std::size_t hop, bool align_sign, F&& fn) {
    const std::size_t N = t.size();
    Signal out(N, 0.0), wsum(N, 0.0);
    if (N == 0) return out;
    win = std::min(win, N);
    hop = std::max<std::size_t>(1, std::min(hop, win));
    std::vector<std::size_t> starts;
    for (std::size_t s = 0; s + win <= N; s += hop) starts.push_back(s);
    if (starts.empty() || starts.back() + win < N) starts.push_back(N - win);
    Signal w(win);
    for (std::size_t i = 0; i < win; ++i)
        w[i] = 0.5 - 0.5 * std::cos(2.0 * M_PI * (static_cast<double>(i) + 0.5) / static_cast<double>(win));
    for (std::size_t s : starts) {
        Signal y = finite_zscore(fn(take(t, s, win)));
        if (align_sign) {
            double c = 0.0;
            for (std::size_t i = 0; i < win; ++i)
                if (wsum[s + i] > 0.0) c += out[s + i] / wsum[s + i] * y[i] * w[i];
            if (c < 0.0)
                for (double& v : y) v = -v;
        }
        for (std::size_t i = 0; i < win; ++i) {
            out[s + i] += w[i] * y[i];
            wsum[s + i] += w[i];
        }
    }
    for (std::size_t i = 0; i < N; ++i) out[i] = wsum[i] > 0.0 ? out[i] / wsum[i] : 0.0;
    return out;
}

// ---- POS: Wang, den Brinker, Stuijk, de Haan, "Algorithmic principles of remote PPG", TBME 2017.
Signal pos(const RgbTrace& t, double win_s) {
    const std::size_t N = t.size();
    Signal H(N, 0.0);
    std::size_t l = static_cast<std::size_t>(std::ceil(win_s * t.fs));
    l = std::clamp<std::size_t>(l, 2, std::max<std::size_t>(2, N));
    if (N < 2) return H;
    Signal s1(l), s2(l);
    for (std::size_t n = l - 1; n < N; ++n) {
        const std::size_t m = n + 1 - l;
        double mr = 0, mg = 0, mb = 0;
        for (std::size_t i = m; i <= n; ++i) {
            mr += t.r[i];
            mg += t.g[i];
            mb += t.b[i];
        }
        mr /= l;
        mg /= l;
        mb /= l;
        if (mr <= 0 || mg <= 0 || mb <= 0) continue;
        double a1 = 0, a2 = 0;
        for (std::size_t i = 0; i < l; ++i) {
            const double rn = t.r[m + i] / mr, gn = t.g[m + i] / mg, bn = t.b[m + i] / mb;
            s1[i] = gn - bn;
            s2[i] = -2.0 * rn + gn + bn;
            a1 += s1[i];
            a2 += s2[i];
        }
        a1 /= l;
        a2 /= l;
        double v1 = 0, v2 = 0;
        for (std::size_t i = 0; i < l; ++i) {
            v1 += (s1[i] - a1) * (s1[i] - a1);
            v2 += (s2[i] - a2) * (s2[i] - a2);
        }
        const double alpha = v2 > 0 ? std::sqrt(v1 / v2) : 0.0;
        double hm = 0;
        for (std::size_t i = 0; i < l; ++i) hm += s1[i] + alpha * s2[i];
        hm /= l;
        for (std::size_t i = 0; i < l; ++i) H[m + i] += s1[i] + alpha * s2[i] - hm;
    }
    return finite_zscore(H);
}

// ---- CHROM: de Haan & Jeanne, "Robust pulse rate from chrominance-based rPPG", TBME 2013.
// Chrominance signals of continuously normalised RGB are band-passed over the whole trace (the
// toolbox port filters inside every 1.6 s window, where the filter transient spans the window), then
// the alpha tuning and Hann overlap-add are done per 1.6 s window as in the paper.
Signal chrom(const RgbTrace& t, double win_s, double lo, double hi) {
    const std::size_t N = t.size();
    Signal S(N, 0.0);
    std::size_t L = static_cast<std::size_t>(std::ceil(win_s * t.fs));
    if (L % 2) ++L;
    if (N < 16) return S;
    L = std::min(L, N - (N % 2));
    const std::size_t half = L / 2;
    const Signal mr = dsp::moving_average(t.r, L), mg = dsp::moving_average(t.g, L), mb = dsp::moving_average(t.b, L);
    Signal xs(N), ys(N);
    for (std::size_t i = 0; i < N; ++i) {
        const double rn = mr[i] > 0 ? t.r[i] / mr[i] : 1.0;
        const double gn = mg[i] > 0 ? t.g[i] / mg[i] : 1.0;
        const double bn = mb[i] > 0 ? t.b[i] / mb[i] : 1.0;
        xs[i] = 3.0 * rn - 2.0 * gn;
        ys[i] = 1.5 * rn + gn - 1.5 * bn;
    }
    const dsp::Sos sos = dsp::butter(3, dsp::BandType::BandPass, lo, std::min(hi, 0.45 * t.fs), t.fs);
    const Signal xf = dsp::sosfiltfilt(sos, xs);
    const Signal yf = dsp::sosfiltfilt(sos, ys);
    Signal hann(L);
    for (std::size_t i = 0; i < L; ++i)  // symmetric Hann (scipy.signal.windows.hann)
        hann[i] = 0.5 - 0.5 * std::cos(2.0 * M_PI * static_cast<double>(i) / static_cast<double>(L - 1));
    std::vector<std::size_t> starts;
    for (std::size_t s = 0; s + L <= N; s += half) starts.push_back(s);
    if (starts.empty() || starts.back() + L < N) starts.push_back(N - L);
    for (std::size_t s : starts) {
        double ax = 0, ay = 0;
        for (std::size_t i = 0; i < L; ++i) {
            ax += xf[s + i];
            ay += yf[s + i];
        }
        ax /= L;
        ay /= L;
        double vx = 0, vy = 0;
        for (std::size_t i = 0; i < L; ++i) {
            vx += (xf[s + i] - ax) * (xf[s + i] - ax);
            vy += (yf[s + i] - ay) * (yf[s + i] - ay);
        }
        const double alpha = vy > 0 ? std::sqrt(vx / vy) : 0.0;
        for (std::size_t i = 0; i < L; ++i) S[s + i] += (xf[s + i] - alpha * yf[s + i]) * hann[i];
    }
    return finite_zscore(S);
}

Mat3 outer_sum(const Window3& w, bool center) {
    const std::size_t n = w.n();
    const Signal* ch[3] = {&w.r, &w.g, &w.b};
    double m[3] = {0, 0, 0};
    if (center)
        for (int c = 0; c < 3; ++c) m[c] = dsp::mean(*ch[c]);
    Mat3 M{};
    for (int a = 0; a < 3; ++a)
        for (int b = a; b < 3; ++b) {
            double acc = 0;
            for (std::size_t i = 0; i < n; ++i) acc += ((*ch[a])[i] - m[a]) * ((*ch[b])[i] - m[b]);
            M[a][b] = M[b][a] = acc;
        }
    return M;
}

// ---- LGI: Pilz et al., "Local group invariance for heart rate estimation", CVPRW 2018.
Signal lgi_window(const Window3& w) {
    Vec3 ev;
    Mat3 V;
    dsp::eig_sym3(outer_sum(w, false), ev, V);
    const Vec3 u = {V[0][0], V[1][0], V[2][0]};
    Signal y(w.n());
    for (std::size_t i = 0; i < w.n(); ++i) {
        const double x[3] = {w.r[i], w.g[i], w.b[i]};
        const double d = u[0] * x[0] + u[1] * x[1] + u[2] * x[2];
        y[i] = x[1] - u[1] * d;  // green row of (I - u u^T) x
    }
    return y;
}

// ---- OMIT: Alvarez Casado & Bordallo Lopez, "Face2PPG", IEEE JBHI 2023 (QR based projection).
Signal omit_window(const Window3& w) {
    double q[3] = {w.r[0], w.g[0], w.b[0]};
    const double nq = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2]);
    Signal y(w.n(), 0.0);
    if (nq <= 0) return y;
    for (double& v : q) v /= nq;
    for (std::size_t i = 0; i < w.n(); ++i) {
        const double x[3] = {w.r[i], w.g[i], w.b[i]};
        const double d = q[0] * x[0] + q[1] * x[1] + q[2] * x[2];
        y[i] = x[1] - q[1] * d;
    }
    return y;
}

// ---- PBV: de Haan & van Leest, "Improved motion robustness of remote-PPG by using the blood
// volume pulse signature", Physiol. Meas. 2014. As in the paper the signature is a fixed
// physiological prior (normalised pulsatile RGB of skin, Wang et al. 2017), not estimated from the
// data: estimating it from channel stds lets strong in-band illumination hijack the projection.
Signal pbv_window(Window3 w) {
    normalize(w);
    const std::size_t n = w.n();
    Signal* ch[3] = {&w.r, &w.g, &w.b};
    for (int c = 0; c < 3; ++c)
        for (double& v : *ch[c]) v -= 1.0;  // AC part of the normalised signal
    Vec3 sig = {0.33, 0.77, 0.53};
    const double den = std::sqrt(sig[0] * sig[0] + sig[1] * sig[1] + sig[2] * sig[2]);
    for (double& v : sig) v /= den;
    Signal y(n, 0.0);
    Mat3 Q = outer_sum(w, false);
    for (int c = 0; c < 3; ++c) Q[c][c] += 1e-12;
    Vec3 W;
    if (!dsp::solve3(Q, sig, W)) return y;
    const double norm = sig[0] * W[0] + sig[1] * W[1] + sig[2] * W[2];
    if (std::abs(norm) < 1e-300) return y;
    for (std::size_t i = 0; i < n; ++i) y[i] = (W[0] * w.r[i] + W[1] * w.g[i] + W[2] * w.b[i]) / norm;
    return y;
}

// ---- PCA: Lewandowska et al., FedCSIS 2011. Most periodic principal component.
Signal pca_window(Window3 w, double fs, double lo, double hi) {
    normalize(w);
    Vec3 ev;
    Mat3 V;
    dsp::eig_sym3(outer_sum(w, true), ev, V);
    Signal best;
    double best_score = -1;
    for (int k = 0; k < 3; ++k) {
        Signal c(w.n());
        for (std::size_t i = 0; i < w.n(); ++i) c[i] = V[0][k] * w.r[i] + V[1][k] * w.g[i] + V[2][k] * w.b[i];
        const double sc = periodicity(c, fs, lo, hi);
        if (sc > best_score) {
            best_score = sc;
            best.swap(c);
        }
    }
    // Pulse lowers green reflectance; orient the component accordingly.
    if (dsp::pearson(best, w.g) < 0)
        for (double& v : best) v = -v;
    return best;
}

// ---- ICA: Poh, McDuff & Picard, Optics Express 2010 / TBME 2011 (JADE).
Signal ica_window(Window3 w, double fs, double lo, double hi) {
    normalize(w);
    const auto S = jade3({w.r, w.g, w.b});
    Signal best;
    double best_score = -1;
    for (const auto& c : S) {
        const double sc = periodicity(c, fs, lo, hi);
        if (sc > best_score) {
            best_score = sc;
            best = c;
        }
    }
    if (dsp::pearson(best, w.g) < 0)
        for (double& v : best) v = -v;
    return best;
}

// ---- 2SR: Wang, Stuijk & de Haan, "A novel algorithm for remote photoplethysmography: spatial
// subspace rotation", TBME 2016. Needs the per-frame skin-pixel correlation matrices.
Signal ssr(const RgbTrace& t) {
    const std::size_t K = t.corr.size();
    Signal P(t.size(), 0.0);
    if (K < 4 || K != t.size()) return P;
    const std::size_t l = std::max<std::size_t>(2, static_cast<std::size_t>(std::lround(t.fs)));
    std::vector<Vec3> L(K);
    std::vector<Mat3> U(K);
    for (std::size_t k = 0; k < K; ++k) {
        dsp::eig_sym3(t.corr[k], L[k], U[k]);
        // Consistent orientation: principal axis (skin colour) has positive components.
        for (int j = 0; j < 3; ++j) {
            const double s = U[k][0][j] + U[k][1][j] + U[k][2][j];
            if ((j == 0 && s < 0) || (j > 0 && U[k][1][j] < 0))
                for (int r = 0; r < 3; ++r) U[k][r][j] = -U[k][r][j];
        }
    }
    Signal s0(l), s1(l);
    for (std::size_t k = l; k < K; ++k) {
        const std::size_t tau = k - l;
        const double l1 = std::max(L[tau][1], 1e-12), l2 = std::max(L[tau][2], 1e-12);
        for (std::size_t z = 0; z < l; ++z) {
            const std::size_t tt = tau + z;
            const double a = std::max(L[tt][0], 0.0);
            const double u0[3] = {U[tt][0][0], U[tt][1][0], U[tt][2][0]};
            const double e[3] = {U[tau][0][1], U[tau][1][1], U[tau][2][1]};
            const double f[3] = {U[tau][0][2], U[tau][1][2], U[tau][2][2]};
            const double de = u0[0] * e[0] + u0[1] * e[1] + u0[2] * e[2];
            const double df = u0[0] * f[0] + u0[1] * f[1] + u0[2] * f[2];
            const double ca = std::sqrt(a / l1) * de, cb = std::sqrt(a / l2) * df;
            s0[z] = ca * e[0] + cb * f[0];
            s1[z] = ca * e[1] + cb * f[1];
        }
        const double sd1 = dsp::stdev(s1);
        const double ratio = sd1 > 0 ? dsp::stdev(s0) / sd1 : 0.0;
        double pm = 0;
        for (std::size_t z = 0; z < l; ++z) pm += s0[z] - ratio * s1[z];
        pm /= l;
        for (std::size_t z = 0; z < l; ++z) P[tau + z] += s0[z] - ratio * s1[z] - pm;
    }
    return finite_zscore(P);
}

double default_window(Method m) {
    switch (m) {
        case Method::POS:
        case Method::CHROM: return 1.6;
        default: return 6.0;
    }
}

}  // namespace

Signal extract_bvp(Method m, const RgbTrace& rgb, const MethodParams& p) {
    const std::size_t N = rgb.size();
    if (N < 4) return Signal(N, 0.0);
    const double win_s = p.window_s > 0 ? p.window_s : default_window(m);
    const std::size_t win = std::max<std::size_t>(8, static_cast<std::size_t>(std::lround(win_s * rgb.fs)));
    const std::size_t hop = std::max<std::size_t>(1, static_cast<std::size_t>(std::lround(rgb.fs)));  // 1 s
    const double fs = rgb.fs, lo = p.hr_lo_hz, hi = p.hr_hi_hz;
    switch (m) {
        case Method::POS: return pos(rgb, win_s);
        case Method::CHROM: return chrom(rgb, win_s, lo, hi);
        case Method::SSR: return ssr(rgb);
        case Method::PRISM: {
            const PrismChoice c = prism_select(rgb);
            return finite_zscore(prism_signal(rgb, c.lambda, c.alpha));
        }
        case Method::Green:
            return window_ola(rgb, win, hop, false, [](Window3 w) {
                normalize(w);
                return w.g;
            });
        case Method::LGI: return window_ola(rgb, win, hop, false, [](const Window3& w) { return lgi_window(w); });
        case Method::OMIT: return window_ola(rgb, win, hop, false, [](const Window3& w) { return omit_window(w); });
        case Method::PBV: return window_ola(rgb, win, hop, true, [](const Window3& w) { return pbv_window(w); });
        case Method::PCA:
            return window_ola(rgb, win, hop, true, [&](const Window3& w) { return pca_window(w, fs, lo, hi); });
        case Method::ICA:
            return window_ola(rgb, win, hop, true, [&](const Window3& w) { return ica_window(w, fs, lo, hi); });
    }
    return Signal(N, 0.0);
}

// JADE for three channels (Cardoso, "High-order contrasts for independent component analysis",
// Neural Computation 1999).
std::vector<Signal> jade3(const std::vector<Signal>& Xin) {
    constexpr int m = 3;
    const std::size_t T = Xin[0].size();
    std::vector<Signal> X = Xin;
    for (auto& row : X) {
        const double mu = dsp::mean(row);
        for (double& v : row) v -= mu;
    }
    // Whitening.
    Mat3 R{};
    for (int a = 0; a < m; ++a)
        for (int b = a; b < m; ++b) {
            double acc = 0;
            for (std::size_t t = 0; t < T; ++t) acc += X[a][t] * X[b][t];
            R[a][b] = R[b][a] = acc / static_cast<double>(T);
        }
    Vec3 ev;
    Mat3 U;
    dsp::eig_sym3(R, ev, U);
    Mat3 W{};
    for (int k = 0; k < m; ++k) {
        const double sc = ev[k] > 1e-20 ? 1.0 / std::sqrt(ev[k]) : 0.0;
        for (int c = 0; c < m; ++c) W[k][c] = sc * U[c][k];
    }
    std::vector<Signal> Z(m, Signal(T, 0.0));
    for (int k = 0; k < m; ++k)
        for (std::size_t t = 0; t < T; ++t) Z[k][t] = W[k][0] * X[0][t] + W[k][1] * X[1][t] + W[k][2] * X[2][t];

    // Fourth-order cumulant matrices (symmetric basis, 6 matrices for m = 3).
    std::vector<Mat3> CM;
    const double invT = 1.0 / static_cast<double>(T);
    for (int im = 0; im < m; ++im) {
        for (int jm = 0; jm <= im; ++jm) {
            Mat3 Q{};
            for (int a = 0; a < m; ++a)
                for (int b = a; b < m; ++b) {
                    double acc = 0;
                    for (std::size_t t = 0; t < T; ++t) acc += Z[im][t] * Z[jm][t] * Z[a][t] * Z[b][t];
                    Q[a][b] = Q[b][a] = acc * invT;
                }
            if (im == jm) {
                for (int a = 0; a < m; ++a) Q[a][a] -= 1.0;
                Q[im][im] -= 2.0;
            } else {
                Q[im][jm] -= 1.0;
                Q[jm][im] -= 1.0;
                for (auto& row : Q)
                    for (double& v : row) v *= std::sqrt(2.0);
            }
            CM.push_back(Q);
        }
    }
    // Joint approximate diagonalisation by Givens rotations.
    Mat3 V = {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
    const double seuil = 1.0 / std::sqrt(static_cast<double>(T)) / 100.0;
    for (int sweep = 0; sweep < 100; ++sweep) {
        bool again = false;
        for (int p = 0; p < m - 1; ++p) {
            for (int q = p + 1; q < m; ++q) {
                double g11 = 0, g12 = 0, g22 = 0;
                for (const auto& M : CM) {
                    const double g1 = M[p][p] - M[q][q];
                    const double g2 = M[p][q] + M[q][p];
                    g11 += g1 * g1;
                    g12 += g1 * g2;
                    g22 += g2 * g2;
                }
                const double ton = g11 - g22, toff = 2.0 * g12;
                const double theta = 0.5 * std::atan2(toff, ton + std::sqrt(ton * ton + toff * toff));
                if (std::abs(theta) <= seuil) continue;
                again = true;
                const double c = std::cos(theta), s = std::sin(theta);
                for (int r = 0; r < m; ++r) {
                    const double vp = V[r][p], vq = V[r][q];
                    V[r][p] = c * vp + s * vq;
                    V[r][q] = -s * vp + c * vq;
                }
                for (auto& M : CM) {
                    for (int j = 0; j < m; ++j) {
                        const double mp = M[p][j], mq = M[q][j];
                        M[p][j] = c * mp + s * mq;
                        M[q][j] = -s * mp + c * mq;
                    }
                    for (int i = 0; i < m; ++i) {
                        const double mp = M[i][p], mq = M[i][q];
                        M[i][p] = c * mp + s * mq;
                        M[i][q] = -s * mp + c * mq;
                    }
                }
            }
        }
        if (!again) break;
    }
    std::vector<Signal> S(m, Signal(T, 0.0));
    for (int k = 0; k < m; ++k)
        for (std::size_t t = 0; t < T; ++t) S[k][t] = V[0][k] * Z[0][t] + V[1][k] * Z[1][t] + V[2][k] * Z[2][t];
    return S;
}

}  // namespace rppg
