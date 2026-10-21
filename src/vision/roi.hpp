// Facial regions of interest on the MediaPipe face mesh and their per-frame colour statistics.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "rppg/types.hpp"
#include "vision/image.hpp"

namespace rppg::vision {

struct RoiDef {
    const char* name;
    std::vector<int> polygon;  // ordered MediaPipe face-mesh vertex indices
};

// The 28 anatomical ROIs of Li, Elgendi & Menon (optimal_roi_rppg, MIT licence), which compared
// all of them under motion / illumination and recommend the glabella, forehead and malar regions.
const std::vector<RoiDef>& facial_rois();
constexpr int kNumFacialRois = 28;
constexpr int kHolistic = kNumFacialRois;  // union of the skin ROIs (no eyes, brows, lips)
constexpr int kNumRegions = kNumFacialRois + 1;
int roi_index(const std::string& name);

// Raw statistics of the accepted (skin) pixels of a region in one frame.
struct RegionStats {
    double sum[3] = {0, 0, 0};     // R, G, B
    double sum2[6] = {0, 0, 0, 0, 0, 0};  // RR, GG, BB, RG, RB, GB
    std::uint32_t n_total = 0;     // pixels inside the polygon
    std::uint32_t n_skin = 0;      // pixels accepted by the skin model
    bool valid = false;
    double mean(int c) const { return n_skin ? sum[c] / n_skin : 0.0; }
};

struct FrameRegions {
    std::array<RegionStats, kNumRegions> region;
    double background[3] = {0, 0, 0};  // mean RGB beside the face (illumination reference)
    bool background_valid = false;
    double face_luma = 0;              // mean luma of the holistic region
    double saturated_fraction = 0;     // clipped pixels inside the face regions
};

// Robust adaptive skin colour model in YCbCr (no fixed thresholds, so it does not exclude dark or
// very light skin): location / scale are histogram medians / MADs of the central facial regions,
// smoothed over time so that the pixel mask does not flicker from frame to frame.
class SkinModel {
public:
    void reset() { ready_ = false; }
    void update(const std::array<std::uint32_t, 256>& hy, const std::array<std::uint32_t, 256>& hcb,
                const std::array<std::uint32_t, 256>& hcr, double rate);
    bool accept(int y, int cb, int cr) const {
        if (!ready_) return true;
        return std::abs(cb - m_[1]) <= t_[1] && std::abs(cr - m_[2]) <= t_[2] && y >= lo_y_ && y <= hi_y_;
    }
    bool ready() const { return ready_; }

private:
    bool ready_ = false;
    double m_[3] = {0, 0, 0}, s_[3] = {0, 0, 0};
    double t_[3] = {0, 0, 0};
    double lo_y_ = 0, hi_y_ = 255;
};

class RoiSampler {
public:
    // landmarks: >= 468 face-mesh points in image pixel coordinates.
    void sample(const RgbImage& img, const std::vector<Point2f>& landmarks, FrameRegions& out);
    void reset() { skin_.reset(); }
    // Polygons of the current frame (for visualisation).
    std::vector<std::vector<Point2f>> polygons(const std::vector<Point2f>& landmarks) const;

private:
    SkinModel skin_;
    std::vector<std::uint32_t> mask_;
};

}  // namespace rppg::vision
