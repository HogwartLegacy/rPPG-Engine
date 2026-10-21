// Face detection (MediaPipe BlazeFace short range), 478-point face mesh (MediaPipe Face Mesh V2)
// and a MediaPipe-style tracker with adaptive landmark smoothing.
#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "nn/ort_model.hpp"
#include "rppg/types.hpp"
#include "vision/image.hpp"

namespace rppg::vision {

struct Detection {
    RectF box;                    // pixels
    std::array<Point2f, 6> kp{};  // right eye, left eye, nose tip, mouth, right ear, left ear
    float score = 0.f;
};

class FaceDetector {
public:
    FaceDetector(const std::string& model_path, int threads);
    std::vector<Detection> detect(const RgbImage& img, float min_score = 0.5f);

private:
    nn::OrtModel model_;
    std::vector<std::array<float, 2>> anchors_;
    std::vector<float> input_, reg_, cls_;
    int size_ = 128;
};

struct MeshResult {
    std::vector<Point2f> pts;  // 478 landmarks, image pixels
    std::vector<float> z;      // relative depth (pixels)
    float presence = 0.f;      // face presence probability
};

class FaceLandmarker {
public:
    FaceLandmarker(const std::string& model_path, int threads);
    bool run(const RgbImage& img, const RotatedRect& roi, MeshResult& out);

private:
    nn::OrtModel model_;
    std::vector<float> input_, lm_, presence_, aux_;
    int size_ = 256;
};

// One-Euro filter (Casiez et al., CHI 2012).
class OneEuro {
public:
    OneEuro(double min_cutoff = 1.0, double beta = 0.0, double d_cutoff = 1.0)
        : min_cutoff_(min_cutoff), beta_(beta), d_cutoff_(d_cutoff) {}
    double operator()(double x, double t);
    void reset() { init_ = false; }

private:
    static double alpha(double dt, double cutoff);
    double min_cutoff_, beta_, d_cutoff_;
    bool init_ = false;
    double x_ = 0, dx_ = 0, t_ = 0;
};

struct TrackerConfig {
    std::string detector_model;
    std::string landmark_model;
    int threads = 1;
    float min_detection_score = 0.5f;
    float min_presence = 0.5f;
    int redetect_interval = 0;      // >0: periodic re-detection (frames) to catch drift
    double smooth_min_cutoff = 1.0;  // One-Euro parameters in face-normalised units
    double smooth_beta = 2.0;
    double smooth_d_cutoff = 1.0;
};

struct TrackedFace {
    bool present = false;
    bool redetected = false;
    float detection_score = 0.f;
    float presence = 0.f;
    std::vector<Point2f> landmarks;  // smoothed (478)
    std::vector<Point2f> raw;        // unsmoothed
    std::vector<float> z;
    RectF box;                       // bounding box of the smoothed landmarks
    RotatedRect roi;
    // Head pose proxies.
    double scale = 0;  // outer eye-corner distance (pixels)
    double roll = 0;   // radians
    double yaw = 0;    // [-1, 1] nose offset between face edges
    double pitch = 0;  // nose height between eyes and chin, centred
    Point2f center;
};

class FaceTracker {
public:
    explicit FaceTracker(const TrackerConfig& cfg);
    const TrackedFace& update(const RgbImage& img, double t);
    const TrackedFace& face() const { return face_; }
    void reset();

private:
    bool landmarks_from_roi(const RgbImage& img, const RotatedRect& roi, MeshResult& m);
    void smooth(const MeshResult& m, double t);

    TrackerConfig cfg_;
    std::unique_ptr<FaceDetector> det_;
    std::unique_ptr<FaceLandmarker> mesh_;
    TrackedFace face_;
    bool tracking_ = false;
    int since_detect_ = 0;
    std::vector<OneEuro> filters_;
};

// ROI conventions of the MediaPipe face landmarker graphs.
RotatedRect roi_from_detection(const Detection& d);
RotatedRect roi_from_landmarks(const std::vector<Point2f>& lm);

}  // namespace rppg::vision
