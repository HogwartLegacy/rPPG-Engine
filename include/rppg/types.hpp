// Basic value types shared by the public API.
#pragma once

#include <cstdint>

namespace rppg {

enum class PixelFormat : int {
    RGB8 = 0,
    BGR8 = 1,   // OpenCV default
    RGBA8 = 2,
    BGRA8 = 3,  // iOS CVPixelBuffer kCVPixelFormatType_32BGRA
    NV21 = 4,   // Android Camera1 / ImageFormat.NV21 (Y plane + interleaved VU)
    NV12 = 5,   // Android Camera2 YUV_420_888 semi-planar, iOS 420YpCbCr8BiPlanar
};

// Non-owning view of a camera frame.
struct ImageView {
    const std::uint8_t* data = nullptr;  // first plane (packed pixels or Y)
    int width = 0;
    int height = 0;
    int stride = 0;                      // bytes per row of the first plane (0 = tightly packed)
    PixelFormat format = PixelFormat::BGR8;
    const std::uint8_t* uv = nullptr;    // interleaved chroma plane for NV12 / NV21
    int uv_stride = 0;
    int rotation = 0;                    // clockwise rotation (0, 90, 180, 270) to apply
};

struct Point2f {
    float x = 0.f, y = 0.f;
};

struct RectF {
    float x = 0.f, y = 0.f, w = 0.f, h = 0.f;
    float cx() const { return x + 0.5f * w; }
    float cy() const { return y + 0.5f * h; }
};

}  // namespace rppg
