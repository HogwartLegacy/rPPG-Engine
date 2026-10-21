#include "vision/roi.hpp"

#include <algorithm>
#include <cmath>
#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace rppg::vision {

namespace {
inline int ctz32(std::uint32_t v) {
#if defined(_MSC_VER)
    unsigned long idx = 0;
    _BitScanForward(&idx, v);
    return static_cast<int>(idx);
#else
    return __builtin_ctz(v);
#endif
}
}  // namespace

const std::vector<RoiDef>& facial_rois() {
    static const std::vector<RoiDef> rois = {
        {"lower medial forehead", {10, 109, 108, 151, 337, 338}},
        {"left lower lateral forehead", {67, 103, 104, 105, 66, 107, 108, 109}},
        {"right lower lateral forehead", {297, 338, 337, 336, 296, 334, 333, 332}},
        {"glabella", {151, 108, 107, 55, 8, 285, 336, 337}},
        {"upper nasal dorsum", {8, 55, 193, 122, 196, 197, 419, 351, 417, 285}},
        {"lower nasal dorsum", {197, 196, 3, 51, 5, 281, 248, 419}},
        {"soft triangle", {4, 45, 134, 220, 237, 44, 1, 274, 457, 440, 363, 275}},
        {"left ala", {134, 131, 49, 102, 64, 219, 218, 237, 220}},
        {"right ala", {363, 440, 457, 438, 439, 294, 331, 279, 360}},
        {"nasal tip", {5, 51, 45, 4, 275, 281}},
        {"left lower nasal sidewall", {3, 217, 126, 209, 131, 134}},
        {"right lower nasal sidewall", {248, 363, 360, 429, 355, 437}},
        {"left mid nasal sidewall", {188, 114, 217, 236, 196}},
        {"right mid nasal sidewall", {412, 419, 456, 437, 343}},
        {"philtrum", {2, 97, 167, 37, 0, 267, 393, 326}},
        {"left upper lip", {97, 165, 185, 40, 39, 37, 167}},
        {"right upper lip", {326, 393, 267, 269, 270, 409, 391}},
        {"left nasolabial fold", {97, 98, 203, 186, 185, 165}},
        {"right nasolabial fold", {326, 391, 409, 410, 423, 327}},
        {"left temporal", {54, 21, 162, 127, 116, 143, 156, 70, 63, 68}},
        {"right temporal", {284, 298, 293, 300, 383, 372, 345, 356, 389, 251}},
        {"left malar", {126, 100, 118, 117, 116, 123, 147, 187, 205, 203, 129, 209}},
        {"right malar", {355, 429, 358, 423, 425, 411, 376, 352, 345, 346, 347, 329}},
        {"left lower cheek", {203, 205, 187, 147, 177, 215, 138, 172, 136, 135, 212, 186, 206}},
        {"right lower cheek", {423, 426, 410, 432, 364, 365, 397, 367, 435, 401, 376, 411, 425}},
        {"chin", {18, 83, 182, 194, 32, 140, 176, 148, 152, 377, 400, 369, 262, 418, 406, 313}},
        {"left marionette fold", {57, 212, 210, 169, 150, 149, 176, 140, 204, 43}},
        {"right marionette fold", {287, 273, 424, 369, 400, 378, 379, 394, 430, 432}},
    };
    return rois;
}

int roi_index(const std::string& name) {
    if (name == "holistic") return kHolistic;
    const auto& r = facial_rois();
    for (std::size_t i = 0; i < r.size(); ++i)
        if (name == r[i].name) return static_cast<int>(i);
    return -1;
}

namespace {

// Skin regions merged into the holistic region: everything except lips, philtrum and the
// marionette folds (beard, mouth motion).
constexpr std::uint32_t kHolisticMask = ~((1u << 14) | (1u << 15) | (1u << 16) | (1u << 26) | (1u << 27)) &
                                        ((1u << kNumFacialRois) - 1u);
// Central regions used to learn the skin colour model.
constexpr std::uint32_t kCoreMask = (1u << 0) | (1u << 3) | (1u << 4) | (1u << 5) | (1u << 21) | (1u << 22);

inline void ycc(int r, int g, int b, int& y, int& cb, int& cr) {
    y = (77 * r + 150 * g + 29 * b + 128) >> 8;
    cb = ((-43 * r - 85 * g + 128 * b + 128) >> 8) + 128;
    cr = ((128 * r - 107 * g - 21 * b + 128) >> 8) + 128;
}

void hist_median_mad(const std::array<std::uint32_t, 256>& h, double& med, double& mad) {
    std::uint64_t total = 0;
    for (auto v : h) total += v;
    med = 128;
    mad = 0;
    if (total == 0) return;
    std::uint64_t acc = 0;
    int m = 0;
    for (; m < 256; ++m) {
        acc += h[m];
        if (2 * acc >= total) break;
    }
    med = m;
    acc = h[m];
    int d = 0;
    while (2 * acc < total && d < 256) {
        ++d;
        if (m - d >= 0) acc += h[m - d];
        if (m + d < 256) acc += h[m + d];
    }
    mad = d;
}

// Scanline fill (pixel centres, even-odd rule) of a polygon given in image coordinates.
template <class F>
void rasterize(const std::vector<Point2f>& poly, int x_min, int y_min, int x_max, int y_max, F&& span) {
    const std::size_t n = poly.size();
    if (n < 3) return;
    float py0 = poly[0].y, py1 = poly[0].y;
    for (const auto& p : poly) {
        py0 = std::min(py0, p.y);
        py1 = std::max(py1, p.y);
    }
    const int y_start = std::max(y_min, static_cast<int>(std::floor(py0)));
    const int y_end = std::min(y_max, static_cast<int>(std::ceil(py1)));
    std::vector<float> xs;
    for (int y = y_start; y <= y_end; ++y) {
        const float yc = y + 0.5f;
        xs.clear();
        for (std::size_t i = 0; i < n; ++i) {
            const Point2f& a = poly[i];
            const Point2f& b = poly[(i + 1) % n];
            if ((a.y <= yc && yc < b.y) || (b.y <= yc && yc < a.y))
                xs.push_back(a.x + (yc - a.y) * (b.x - a.x) / (b.y - a.y));
        }
        std::sort(xs.begin(), xs.end());
        for (std::size_t k = 0; k + 1 < xs.size(); k += 2) {
            const int xa = std::max(x_min, static_cast<int>(std::ceil(xs[k] - 0.5f)));
            const int xb = std::min(x_max, static_cast<int>(std::floor(xs[k + 1] - 0.5f)));
            if (xb >= xa) span(y, xa, xb);
        }
    }
}

}  // namespace

void SkinModel::update(const std::array<std::uint32_t, 256>& hy, const std::array<std::uint32_t, 256>& hcb,
                       const std::array<std::uint32_t, 256>& hcr, double rate) {
    double m[3], s[3];
    hist_median_mad(hy, m[0], s[0]);
    hist_median_mad(hcb, m[1], s[1]);
    hist_median_mad(hcr, m[2], s[2]);
    for (int c = 0; c < 3; ++c) s[c] *= 1.4826;  // MAD -> sigma
    if (!ready_) {
        for (int c = 0; c < 3; ++c) m_[c] = m[c], s_[c] = s[c];
        ready_ = true;
    } else {
        for (int c = 0; c < 3; ++c) {
            m_[c] += rate * (m[c] - m_[c]);
            s_[c] += rate * (s[c] - s_[c]);
        }
    }
    t_[1] = std::max(3.0 * s_[1], 5.0) + 2.0;
    t_[2] = std::max(3.0 * s_[2], 5.0) + 2.0;
    lo_y_ = m_[0] - std::max(3.5 * s_[0], 30.0);
    hi_y_ = std::min(m_[0] + std::max(3.0 * s_[0], 30.0), 250.0);
}

std::vector<std::vector<Point2f>> RoiSampler::polygons(const std::vector<Point2f>& lm) const {
    std::vector<std::vector<Point2f>> out;
    if (lm.size() < 468) return out;
    for (const auto& r : facial_rois()) {
        std::vector<Point2f> p;
        for (int i : r.polygon) p.push_back(lm[static_cast<std::size_t>(i)]);
        out.push_back(std::move(p));
    }
    return out;
}

void RoiSampler::sample(const RgbImage& img, const std::vector<Point2f>& lm, FrameRegions& out) {
    out = FrameRegions{};
    if (lm.size() < 468 || img.empty()) return;
    float fx0 = 1e9f, fy0 = 1e9f, fx1 = -1e9f, fy1 = -1e9f;
    for (std::size_t i = 0; i < 468; ++i) {
        fx0 = std::min(fx0, lm[i].x);
        fy0 = std::min(fy0, lm[i].y);
        fx1 = std::max(fx1, lm[i].x);
        fy1 = std::max(fy1, lm[i].y);
    }
    const int x0 = std::max(0, static_cast<int>(std::floor(fx0)) - 1);
    const int y0 = std::max(0, static_cast<int>(std::floor(fy0)) - 1);
    const int x1 = std::min(img.width - 1, static_cast<int>(std::ceil(fx1)) + 1);
    const int y1 = std::min(img.height - 1, static_cast<int>(std::ceil(fy1)) + 1);
    if (x1 <= x0 || y1 <= y0) return;
    const int bw = x1 - x0 + 1, bh = y1 - y0 + 1;
    mask_.assign(static_cast<std::size_t>(bw) * bh, 0u);

    const auto& rois = facial_rois();
    std::vector<Point2f> poly;
    for (std::size_t r = 0; r < rois.size(); ++r) {
        poly.clear();
        for (int i : rois[r].polygon) poly.push_back(lm[static_cast<std::size_t>(i)]);
        const std::uint32_t bit = 1u << r;
        rasterize(poly, x0, y0, x1, y1, [&](int y, int xa, int xb) {
            std::uint32_t* m = mask_.data() + static_cast<std::size_t>(y - y0) * bw - x0;
            for (int x = xa; x <= xb; ++x) m[x] |= bit;
        });
    }

    // 1) Update the skin colour model from the central regions (every other pixel).
    std::array<std::uint32_t, 256> hy{}, hcb{}, hcr{};
    std::uint32_t nh = 0;
    for (int y = y0; y <= y1; y += 2) {
        const std::uint8_t* row = img.row(y);
        const std::uint32_t* m = mask_.data() + static_cast<std::size_t>(y - y0) * bw;
        for (int x = x0; x <= x1; x += 2) {
            if (!(m[x - x0] & kCoreMask)) continue;
            const std::uint8_t* p = row + x * 3;
            int Y, cb, cr;
            ycc(p[0], p[1], p[2], Y, cb, cr);
            ++hy[std::clamp(Y, 0, 255)];
            ++hcb[std::clamp(cb, 0, 255)];
            ++hcr[std::clamp(cr, 0, 255)];
            ++nh;
        }
    }
    if (nh > 50) skin_.update(hy, hcb, hcr, skin_.ready() ? 0.05 : 1.0);

    // 2) Accumulate statistics of accepted pixels per region (each pixel visited once).
    std::uint32_t n_face = 0, n_sat = 0;
    double luma_acc = 0;
    for (int y = y0; y <= y1; ++y) {
        const std::uint8_t* row = img.row(y);
        const std::uint32_t* m = mask_.data() + static_cast<std::size_t>(y - y0) * bw;
        for (int x = x0; x <= x1; ++x) {
            const std::uint32_t bits = m[x - x0];
            if (!bits) continue;
            const std::uint8_t* p = row + x * 3;
            const int R = p[0], G = p[1], B = p[2];
            int Y, cb, cr;
            ycc(R, G, B, Y, cb, cr);
            const bool sat = R >= 254 || G >= 254 || B >= 254;
            const bool holo = (bits & kHolisticMask) != 0;
            if (holo) {
                ++n_face;
                n_sat += sat;
            }
            const bool skin = !sat && R > 2 && G > 2 && B > 2 && skin_.accept(Y, cb, cr);
            const double rr = R * R, gg = G * G, bb = B * B, rg = R * G, rb = R * B, gb = G * B;
            auto add = [&](RegionStats& s) {
                ++s.n_total;
                if (!skin) return;
                ++s.n_skin;
                s.sum[0] += R;
                s.sum[1] += G;
                s.sum[2] += B;
                s.sum2[0] += rr;
                s.sum2[1] += gg;
                s.sum2[2] += bb;
                s.sum2[3] += rg;
                s.sum2[4] += rb;
                s.sum2[5] += gb;
            };
            for (std::uint32_t b = bits; b; b &= b - 1) {
                const int idx = ctz32(b);
                add(out.region[static_cast<std::size_t>(idx)]);
            }
            if (holo) {
                add(out.region[kHolistic]);
                if (skin) luma_acc += Y;
            }
        }
    }
    for (auto& s : out.region) s.valid = s.n_skin >= 16 && s.n_skin * 4 >= s.n_total;
    const auto& h = out.region[kHolistic];
    out.face_luma = h.n_skin ? luma_acc / h.n_skin : 0.0;
    out.saturated_fraction = n_face ? static_cast<double>(n_sat) / n_face : 0.0;

    // 3) Background strips left / right of the face: illumination reference.
    const double fw = fx1 - fx0, fh = fy1 - fy0;
    double acc[3] = {0, 0, 0};
    std::uint32_t nb = 0;
    const int by0 = std::max(0, static_cast<int>(fy0)), by1 = std::min(img.height - 1, static_cast<int>(fy0 + 0.6 * fh));
    for (int side = 0; side < 2; ++side) {
        const double a = side == 0 ? fx0 - 0.6 * fw : fx1 + 0.2 * fw;
        const int bx0 = std::max(0, static_cast<int>(a)), bx1 = std::min(img.width - 1, static_cast<int>(a + 0.4 * fw));
        for (int y = by0; y <= by1; y += 2) {
            const std::uint8_t* row = img.row(y);
            for (int x = bx0; x <= bx1; x += 2) {
                acc[0] += row[x * 3];
                acc[1] += row[x * 3 + 1];
                acc[2] += row[x * 3 + 2];
                ++nb;
            }
        }
    }
    if (nb > 100) {
        for (int c = 0; c < 3; ++c) out.background[c] = acc[c] / nb;
        out.background_valid = true;
    }
}

}  // namespace rppg::vision
