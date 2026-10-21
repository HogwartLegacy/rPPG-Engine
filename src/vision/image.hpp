// Minimal image utilities so the engine core does not depend on OpenCV.
#pragma once

#include <cstdint>
#include <vector>

#include "rppg/types.hpp"

namespace rppg::vision {

struct RgbImage {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> data;  // packed RGB, stride = 3 * width
    bool empty() const { return width <= 0 || height <= 0; }
    const std::uint8_t* row(int y) const { return data.data() + static_cast<std::size_t>(y) * width * 3; }
    std::uint8_t* row(int y) { return data.data() + static_cast<std::size_t>(y) * width * 3; }
};

// Converts (and rotates) a frame to packed RGB8. If max_side > 0 the result is box-downscaled by
// an integer factor so that max(width, height) <= max_side. Returns the applied downscale factor
// (>= 1) or 0 on invalid input.
int to_rgb(const ImageView& in, RgbImage& out, int max_side = 0);

// Affine map from crop pixel coordinates (u, v) to image coordinates.
struct Affine {
    double a = 1, b = 0, tx = 0;  // x = a u + b v + tx
    double c = 0, d = 1, ty = 0;  // y = c u + d v + ty
    Point2f apply(double u, double v) const {
        return {static_cast<float>(a * u + b * v + tx), static_cast<float>(c * u + d * v + ty)};
    }
};

// Rotated square ROI (MediaPipe NormalizedRect semantics, pixel units).
struct RotatedRect {
    double cx = 0, cy = 0, size = 0, angle = 0;  // angle in radians, clockwise in image coordinates
    // Maps normalised ROI coordinates (0..1, 0..1) to image pixels.
    Point2f to_image(double nx, double ny) const;
    // Affine for an out_w x out_h crop whose pixel (u, v) has its centre at (u + .5, v + .5).
    Affine crop_affine(int out_w, int out_h) const;
};

// Bilinear warp into a float HWC tensor: value = pixel / 255 * scale + offset (outside -> pad).
void warp_to_tensor(const RgbImage& img, const Affine& M, int w, int h, float scale, float offset, float pad,
                    float* dst);

// Letterbox (aspect preserving, centred) resize into a size x size float tensor.
struct Letterbox {
    double scale = 1, ox = 0, oy = 0;  // tensor = image * scale + offset
};
Letterbox letterbox_to_tensor(const RgbImage& img, int size, float scale, float offset, float* dst);

// Area-average resample of the axis-aligned box [x0, x1) x [y0, y1) (clamped to the image) to
// w x h RGB floats in [0, 1] (OpenCV INTER_AREA semantics for down-scaling).
void crop_resize_area(const RgbImage& img, double x0, double y0, double x1, double y1, int w, int h, float* dst);

}  // namespace rppg::vision
