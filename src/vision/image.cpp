#include "vision/image.hpp"

#include <algorithm>
#include <cmath>

namespace rppg::vision {

namespace {

inline std::uint8_t clamp_u8(double v) { return static_cast<std::uint8_t>(std::clamp(v + 0.5, 0.0, 255.0)); }

// BT.601 full-range YCbCr -> RGB (JFIF; Android NV21 and iOS full-range buffers).
inline void yuv_to_rgb(int Y, int U, int V, std::uint8_t* px) {
    const double y = Y, u = U - 128.0, v = V - 128.0;
    px[0] = clamp_u8(y + 1.402 * v);
    px[1] = clamp_u8(y - 0.344136 * u - 0.714136 * v);
    px[2] = clamp_u8(y + 1.772 * u);
}

}  // namespace

int to_rgb(const ImageView& in, RgbImage& out, int max_side) {
    if (!in.data || in.width <= 0 || in.height <= 0) return 0;
    const int W = in.width, H = in.height;
    const bool yuv = in.format == PixelFormat::NV12 || in.format == PixelFormat::NV21;
    if (yuv && !in.uv) return 0;
    const int bpp = (in.format == PixelFormat::RGB8 || in.format == PixelFormat::BGR8) ? 3 : (yuv ? 1 : 4);
    const int stride = in.stride > 0 ? in.stride : W * bpp;
    const int uv_stride = in.uv_stride > 0 ? in.uv_stride : W;
    const int rot = ((in.rotation % 360) + 360) % 360;
    const bool swap = rot == 90 || rot == 270;
    const int OW = swap ? H : W, OH = swap ? W : H;

    RgbImage full;
    RgbImage& dst = (max_side > 0 && std::max(OW, OH) > max_side) ? full : out;
    dst.width = OW;
    dst.height = OH;
    dst.data.resize(static_cast<std::size_t>(OW) * OH * 3);

    auto fetch = [&](int x, int y, std::uint8_t* px) {
        switch (in.format) {
            case PixelFormat::RGB8: {
                const std::uint8_t* s = in.data + static_cast<std::size_t>(y) * stride + x * 3;
                px[0] = s[0], px[1] = s[1], px[2] = s[2];
                break;
            }
            case PixelFormat::BGR8: {
                const std::uint8_t* s = in.data + static_cast<std::size_t>(y) * stride + x * 3;
                px[0] = s[2], px[1] = s[1], px[2] = s[0];
                break;
            }
            case PixelFormat::RGBA8: {
                const std::uint8_t* s = in.data + static_cast<std::size_t>(y) * stride + x * 4;
                px[0] = s[0], px[1] = s[1], px[2] = s[2];
                break;
            }
            case PixelFormat::BGRA8: {
                const std::uint8_t* s = in.data + static_cast<std::size_t>(y) * stride + x * 4;
                px[0] = s[2], px[1] = s[1], px[2] = s[0];
                break;
            }
            case PixelFormat::NV12:
            case PixelFormat::NV21: {
                const int Y = in.data[static_cast<std::size_t>(y) * stride + x];
                const std::uint8_t* c = in.uv + static_cast<std::size_t>(y / 2) * uv_stride + (x / 2) * 2;
                const int U = in.format == PixelFormat::NV12 ? c[0] : c[1];
                const int V = in.format == PixelFormat::NV12 ? c[1] : c[0];
                yuv_to_rgb(Y, U, V, px);
                break;
            }
        }
    };

    for (int yo = 0; yo < OH; ++yo) {
        std::uint8_t* row = dst.row(yo);
        for (int xo = 0; xo < OW; ++xo) {
            int x, y;
            switch (rot) {
                case 90: x = yo, y = H - 1 - xo; break;
                case 180: x = W - 1 - xo, y = H - 1 - yo; break;
                case 270: x = W - 1 - yo, y = xo; break;
                default: x = xo, y = yo; break;
            }
            fetch(x, y, row + xo * 3);
        }
    }
    if (&dst == &out) return 1;

    // Integer box down-scaling keeps sensor noise averaging (better than decimation for rPPG).
    const int f = (std::max(OW, OH) + max_side - 1) / max_side;
    out.width = OW / f;
    out.height = OH / f;
    out.data.assign(static_cast<std::size_t>(out.width) * out.height * 3, 0);
    const double inv = 1.0 / (f * f);
    for (int y = 0; y < out.height; ++y) {
        std::uint8_t* o = out.row(y);
        for (int x = 0; x < out.width; ++x) {
            double acc[3] = {0, 0, 0};
            for (int dy = 0; dy < f; ++dy) {
                const std::uint8_t* s = full.row(y * f + dy) + x * f * 3;
                for (int dx = 0; dx < f; ++dx)
                    for (int c = 0; c < 3; ++c) acc[c] += s[dx * 3 + c];
            }
            for (int c = 0; c < 3; ++c) o[x * 3 + c] = clamp_u8(acc[c] * inv);
        }
    }
    return f;
}

Point2f RotatedRect::to_image(double nx, double ny) const {
    const double c = std::cos(angle), s = std::sin(angle);
    const double dx = (nx - 0.5) * size, dy = (ny - 0.5) * size;
    return {static_cast<float>(cx + c * dx - s * dy), static_cast<float>(cy + s * dx + c * dy)};
}

Affine RotatedRect::crop_affine(int out_w, int out_h) const {
    const double c = std::cos(angle), s = std::sin(angle);
    Affine M;
    M.a = size * c / out_w;
    M.b = -size * s / out_h;
    M.c = size * s / out_w;
    M.d = size * c / out_h;
    const double u0 = (0.5 / out_w - 0.5) * size, v0 = (0.5 / out_h - 0.5) * size;
    M.tx = cx + c * u0 - s * v0;
    M.ty = cy + s * u0 + c * v0;
    return M;
}

void warp_to_tensor(const RgbImage& img, const Affine& M, int w, int h, float scale, float offset, float pad,
                    float* dst) {
    const float k = scale / 255.f;
    const int W = img.width, H = img.height;
    for (int v = 0; v < h; ++v) {
        for (int u = 0; u < w; ++u) {
            const double X = M.a * u + M.b * v + M.tx;
            const double Y = M.c * u + M.d * v + M.ty;
            float* o = dst + (static_cast<std::size_t>(v) * w + u) * 3;
            if (X < 0 || Y < 0 || X > W || Y > H) {
                o[0] = o[1] = o[2] = pad;
                continue;
            }
            const double fx = X - 0.5, fy = Y - 0.5;
            const int x0 = static_cast<int>(std::floor(fx)), y0 = static_cast<int>(std::floor(fy));
            const double wx = fx - x0, wy = fy - y0;
            const int xa = std::clamp(x0, 0, W - 1), xb = std::clamp(x0 + 1, 0, W - 1);
            const int ya = std::clamp(y0, 0, H - 1), yb = std::clamp(y0 + 1, 0, H - 1);
            const std::uint8_t* p00 = img.row(ya) + xa * 3;
            const std::uint8_t* p01 = img.row(ya) + xb * 3;
            const std::uint8_t* p10 = img.row(yb) + xa * 3;
            const std::uint8_t* p11 = img.row(yb) + xb * 3;
            for (int ch = 0; ch < 3; ++ch) {
                const double top = p00[ch] * (1 - wx) + p01[ch] * wx;
                const double bot = p10[ch] * (1 - wx) + p11[ch] * wx;
                o[ch] = static_cast<float>(top * (1 - wy) + bot * wy) * k + offset;
            }
        }
    }
}

Letterbox letterbox_to_tensor(const RgbImage& img, int size, float scale, float offset, float* dst) {
    Letterbox lb;
    lb.scale = std::min(static_cast<double>(size) / img.width, static_cast<double>(size) / img.height);
    lb.ox = (size - img.width * lb.scale) / 2.0;
    lb.oy = (size - img.height * lb.scale) / 2.0;
    Affine M;
    M.a = 1.0 / lb.scale;
    M.d = 1.0 / lb.scale;
    M.tx = (0.5 - lb.ox) / lb.scale;
    M.ty = (0.5 - lb.oy) / lb.scale;
    warp_to_tensor(img, M, size, size, scale, offset, offset, dst);
    return lb;
}

void crop_resize_area(const RgbImage& img, double x0, double y0, double x1, double y1, int w, int h, float* dst) {
    x0 = std::clamp(x0, 0.0, static_cast<double>(img.width));
    x1 = std::clamp(x1, 0.0, static_cast<double>(img.width));
    y0 = std::clamp(y0, 0.0, static_cast<double>(img.height));
    y1 = std::clamp(y1, 0.0, static_cast<double>(img.height));
    if (x1 - x0 < 1.0 || y1 - y0 < 1.0) {
        std::fill(dst, dst + static_cast<std::size_t>(w) * h * 3, 0.f);
        return;
    }
    struct Tap {
        int idx;
        double wgt;
    };
    auto taps = [](double a0, double a1, int n, int limit) {
        std::vector<std::vector<Tap>> t(n);
        const double step = (a1 - a0) / n;
        for (int j = 0; j < n; ++j) {
            const double a = a0 + j * step, b = a + step;
            for (int i = static_cast<int>(std::floor(a)); i < static_cast<int>(std::ceil(b)); ++i) {
                const double ov = std::min(b, i + 1.0) - std::max(a, static_cast<double>(i));
                if (ov > 1e-9) t[j].push_back({std::clamp(i, 0, limit - 1), ov});
            }
        }
        return t;
    };
    const auto tx = taps(x0, x1, w, img.width);
    const auto ty = taps(y0, y1, h, img.height);
    for (int v = 0; v < h; ++v) {
        for (int u = 0; u < w; ++u) {
            double acc[3] = {0, 0, 0}, wsum = 0;
            for (const Tap& ry : ty[v]) {
                const std::uint8_t* row = img.row(ry.idx);
                for (const Tap& rx : tx[u]) {
                    const double wt = ry.wgt * rx.wgt;
                    const std::uint8_t* p = row + rx.idx * 3;
                    acc[0] += wt * p[0];
                    acc[1] += wt * p[1];
                    acc[2] += wt * p[2];
                    wsum += wt;
                }
            }
            float* o = dst + (static_cast<std::size_t>(v) * w + u) * 3;
            const double inv = wsum > 0 ? 1.0 / (255.0 * wsum) : 0.0;
            for (int c = 0; c < 3; ++c) o[c] = static_cast<float>(acc[c] * inv);
        }
    }
}

}  // namespace rppg::vision
