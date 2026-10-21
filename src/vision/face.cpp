#include "vision/face.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rppg::vision {

namespace {

float sigmoid(float x) { return 1.f / (1.f + std::exp(-std::clamp(x, -80.f, 80.f))); }

float iou(const RectF& a, const RectF& b) {
    const float x0 = std::max(a.x, b.x), y0 = std::max(a.y, b.y);
    const float x1 = std::min(a.x + a.w, b.x + b.w), y1 = std::min(a.y + a.h, b.y + b.h);
    const float inter = std::max(0.f, x1 - x0) * std::max(0.f, y1 - y0);
    const float uni = a.w * a.h + b.w * b.h - inter;
    return uni > 0 ? inter / uni : 0.f;
}

double dist(const Point2f& a, const Point2f& b) { return std::hypot(double(a.x) - b.x, double(a.y) - b.y); }

}  // namespace

// ------------------------------------------------------------------------------------------------
// FaceDetector: MediaPipe face_detection_short_range (128x128, 896 SSD anchors).
// ------------------------------------------------------------------------------------------------
FaceDetector::FaceDetector(const std::string& model_path, int threads) : model_(model_path, threads) {
    if (model_.inputs().size() != 1 || model_.inputs()[0].elements != 128u * 128u * 3u || model_.outputs().size() != 2)
        throw std::runtime_error("unexpected face detector signature: " + model_path);
    for (auto [stride, per_cell] : {std::pair<int, int>{8, 2}, {16, 6}}) {
        const int fm = size_ / stride;
        for (int y = 0; y < fm; ++y)
            for (int x = 0; x < fm; ++x)
                for (int k = 0; k < per_cell; ++k) anchors_.push_back({(x + 0.5f) / fm, (y + 0.5f) / fm});
    }
    input_.resize(model_.inputs()[0].elements);
    // Outputs: regressors [896 x 16], classificators [896 x 1] (identified by size).
    const auto& o = model_.outputs();
    reg_.resize(o[0].elements > o[1].elements ? o[0].elements : o[1].elements);
    cls_.resize(o[0].elements > o[1].elements ? o[1].elements : o[0].elements);
    if (reg_.size() != anchors_.size() * 16 || cls_.size() != anchors_.size())
        throw std::runtime_error("face detector output size mismatch");
}

std::vector<Detection> FaceDetector::detect(const RgbImage& img, float min_score) {
    std::vector<Detection> out;
    if (img.empty()) return out;
    const Letterbox lb = letterbox_to_tensor(img, size_, 2.f, -1.f, input_.data());
    const auto& o = model_.outputs();
    const bool reg_first = o[0].elements > o[1].elements;
    model_.run({input_.data()}, reg_first ? std::vector<float*>{reg_.data(), cls_.data()}
                                          : std::vector<float*>{cls_.data(), reg_.data()});
    auto to_img_x = [&](float nx) { return static_cast<float>((nx * size_ - lb.ox) / lb.scale); };
    auto to_img_y = [&](float ny) { return static_cast<float>((ny * size_ - lb.oy) / lb.scale); };
    std::vector<Detection> cand;
    for (std::size_t i = 0; i < anchors_.size(); ++i) {
        const float sc = sigmoid(cls_[i]);
        if (sc < min_score) continue;
        const float* r = reg_.data() + i * 16;
        const float cx = r[0] / size_ + anchors_[i][0], cy = r[1] / size_ + anchors_[i][1];
        const float w = r[2] / size_, h = r[3] / size_;
        Detection d;
        d.score = sc;
        const float x0 = to_img_x(cx - w / 2), y0 = to_img_y(cy - h / 2);
        const float x1 = to_img_x(cx + w / 2), y1 = to_img_y(cy + h / 2);
        d.box = {x0, y0, x1 - x0, y1 - y0};
        for (int k = 0; k < 6; ++k)
            d.kp[k] = {to_img_x(r[4 + 2 * k] / size_ + anchors_[i][0]), to_img_y(r[5 + 2 * k] / size_ + anchors_[i][1])};
        cand.push_back(d);
    }
    // Weighted non-maximum suppression (MediaPipe NonMaxSuppressionCalculator, WEIGHTED).
    std::sort(cand.begin(), cand.end(), [](const Detection& a, const Detection& b) { return a.score > b.score; });
    std::vector<char> used(cand.size(), 0);
    for (std::size_t i = 0; i < cand.size(); ++i) {
        if (used[i]) continue;
        Detection acc{};
        float wsum = 0;
        for (std::size_t j = i; j < cand.size(); ++j) {
            if (used[j] || iou(cand[i].box, cand[j].box) < 0.3f) continue;
            used[j] = 1;
            const float w = cand[j].score;
            wsum += w;
            acc.box.x += w * cand[j].box.x;
            acc.box.y += w * cand[j].box.y;
            acc.box.w += w * cand[j].box.w;
            acc.box.h += w * cand[j].box.h;
            for (int k = 0; k < 6; ++k) {
                acc.kp[k].x += w * cand[j].kp[k].x;
                acc.kp[k].y += w * cand[j].kp[k].y;
            }
        }
        acc.box.x /= wsum;
        acc.box.y /= wsum;
        acc.box.w /= wsum;
        acc.box.h /= wsum;
        for (auto& p : acc.kp) {
            p.x /= wsum;
            p.y /= wsum;
        }
        acc.score = cand[i].score;
        out.push_back(acc);
    }
    return out;
}

// ------------------------------------------------------------------------------------------------
// FaceLandmarker: MediaPipe Face Mesh V2 (256x256 RGB in [0, 1], 478 x 3 landmarks).
// ------------------------------------------------------------------------------------------------
FaceLandmarker::FaceLandmarker(const std::string& model_path, int threads) : model_(model_path, threads) {
    if (model_.inputs().size() != 1 || model_.inputs()[0].elements != 256u * 256u * 3u)
        throw std::runtime_error("unexpected face landmark model signature: " + model_path);
    input_.resize(model_.inputs()[0].elements);
}

bool FaceLandmarker::run(const RgbImage& img, const RotatedRect& roi, MeshResult& out) {
    if (img.empty() || roi.size < 8) return false;
    warp_to_tensor(img, roi.crop_affine(size_, size_), size_, size_, 1.f, 0.f, 0.f, input_.data());
    const auto& o = model_.outputs();
    std::vector<std::vector<float>> bufs(o.size());
    std::vector<float*> ptrs;
    int lm_idx = -1, pres_idx = -1;
    for (std::size_t i = 0; i < o.size(); ++i) {
        bufs[i].resize(o[i].elements);
        ptrs.push_back(bufs[i].data());
        if (o[i].elements == 478u * 3u)
            lm_idx = static_cast<int>(i);
        else if (o[i].elements == 1 && pres_idx < 0)
            pres_idx = static_cast<int>(i);
    }
    if (lm_idx < 0) throw std::runtime_error("face landmark model: no 478x3 output");
    model_.run({input_.data()}, ptrs);
    out.presence = pres_idx >= 0 ? sigmoid(bufs[pres_idx][0]) : 1.f;
    const float* L = bufs[lm_idx].data();
    out.pts.resize(478);
    out.z.resize(478);
    for (int i = 0; i < 478; ++i) {
        out.pts[i] = roi.to_image(L[3 * i] / size_, L[3 * i + 1] / size_);
        out.z[i] = static_cast<float>(L[3 * i + 2] * roi.size / size_);
    }
    return true;
}

// ------------------------------------------------------------------------------------------------
double OneEuro::alpha(double dt, double cutoff) {
    const double tau = 1.0 / (2.0 * M_PI * cutoff);
    return 1.0 / (1.0 + tau / dt);
}

double OneEuro::operator()(double x, double t) {
    if (!init_) {
        init_ = true;
        x_ = x;
        dx_ = 0;
        t_ = t;
        return x;
    }
    const double dt = std::max(t - t_, 1e-3);
    t_ = t;
    const double dx = (x - x_) / dt;
    const double ad = alpha(dt, d_cutoff_);
    dx_ = ad * dx + (1 - ad) * dx_;
    const double a = alpha(dt, min_cutoff_ + beta_ * std::abs(dx_));
    x_ = a * x + (1 - a) * x_;
    return x_;
}

// ------------------------------------------------------------------------------------------------
RotatedRect roi_from_detection(const Detection& d) {
    RotatedRect r;
    r.cx = d.box.cx();
    r.cy = d.box.cy();
    r.size = std::max(d.box.w, d.box.h) * 1.5;
    r.angle = std::atan2(double(d.kp[1].y) - d.kp[0].y, double(d.kp[1].x) - d.kp[0].x);
    return r;
}

RotatedRect roi_from_landmarks(const std::vector<Point2f>& lm) {
    RotatedRect r;
    if (lm.size() < 468) return r;
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    for (const auto& p : lm) {
        x0 = std::min(x0, p.x);
        y0 = std::min(y0, p.y);
        x1 = std::max(x1, p.x);
        y1 = std::max(y1, p.y);
    }
    r.cx = 0.5 * (x0 + x1);
    r.cy = 0.5 * (y0 + y1);
    r.size = std::max(x1 - x0, y1 - y0) * 1.5;
    r.angle = std::atan2(double(lm[263].y) - lm[33].y, double(lm[263].x) - lm[33].x);
    return r;
}

FaceTracker::FaceTracker(const TrackerConfig& cfg) : cfg_(cfg) {
    det_ = std::make_unique<FaceDetector>(cfg.detector_model, cfg.threads);
    mesh_ = std::make_unique<FaceLandmarker>(cfg.landmark_model, cfg.threads);
}

void FaceTracker::reset() {
    tracking_ = false;
    since_detect_ = 0;
    filters_.clear();
    face_ = TrackedFace{};
}

bool FaceTracker::landmarks_from_roi(const RgbImage& img, const RotatedRect& roi, MeshResult& m) {
    if (!mesh_->run(img, roi, m)) return false;
    face_.roi = roi;
    return m.presence >= cfg_.min_presence;
}

void FaceTracker::smooth(const MeshResult& m, double t) {
    const double s = std::max(dist(m.pts[33], m.pts[263]), 1.0);
    if (filters_.size() != m.pts.size() * 2 || face_.scale <= 0) {
        filters_.assign(m.pts.size() * 2, OneEuro(cfg_.smooth_min_cutoff, cfg_.smooth_beta, cfg_.smooth_d_cutoff));
        face_.scale = s;
    }
    face_.scale += 0.05 * (s - face_.scale);  // slowly varying normaliser
    const double S = face_.scale;
    face_.landmarks.resize(m.pts.size());
    for (std::size_t i = 0; i < m.pts.size(); ++i) {
        face_.landmarks[i].x = static_cast<float>(filters_[2 * i](m.pts[i].x / S, t) * S);
        face_.landmarks[i].y = static_cast<float>(filters_[2 * i + 1](m.pts[i].y / S, t) * S);
    }
}

const TrackedFace& FaceTracker::update(const RgbImage& img, double t) {
    face_.redetected = false;
    MeshResult m;
    bool ok = false;
    if (tracking_ && (cfg_.redetect_interval <= 0 || since_detect_ < cfg_.redetect_interval)) {
        ok = landmarks_from_roi(img, roi_from_landmarks(face_.raw), m);
        ++since_detect_;
    }
    if (!ok) {
        const auto dets = det_->detect(img, cfg_.min_detection_score);
        if (!dets.empty()) {
            std::size_t best = 0;
            float best_score = -1.f;
            for (std::size_t i = 0; i < dets.size(); ++i) {
                // Prefer the tracked subject; otherwise the most confident, largest face.
                float sc = dets[i].score * std::sqrt(std::max(1.f, dets[i].box.w * dets[i].box.h));
                if (tracking_ || face_.present) sc *= 0.2f + iou(dets[i].box, face_.box);
                if (sc > best_score) {
                    best_score = sc;
                    best = i;
                }
            }
            ok = landmarks_from_roi(img, roi_from_detection(dets[best]), m);
            face_.detection_score = dets[best].score;
            face_.redetected = true;
            since_detect_ = 0;
            if (ok && !tracking_) filters_.clear();
        }
    }
    if (!ok) {
        tracking_ = false;
        face_.present = false;
        filters_.clear();
        face_.scale = 0;
        return face_;
    }
    tracking_ = true;
    face_.present = true;
    face_.presence = m.presence;
    face_.raw = m.pts;
    face_.z = m.z;
    smooth(m, t);

    const auto& L = face_.landmarks;
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    double sx = 0, sy = 0;
    for (const auto& p : L) {
        x0 = std::min(x0, p.x);
        y0 = std::min(y0, p.y);
        x1 = std::max(x1, p.x);
        y1 = std::max(y1, p.y);
        sx += p.x;
        sy += p.y;
    }
    face_.box = {x0, y0, x1 - x0, y1 - y0};
    face_.center = {static_cast<float>(sx / L.size()), static_cast<float>(sy / L.size())};
    face_.roll = std::atan2(double(L[263].y) - L[33].y, double(L[263].x) - L[33].x);
    const double dl = dist(L[1], L[234]), dr = dist(L[1], L[454]);
    face_.yaw = (dl + dr) > 0 ? (dl - dr) / (dl + dr) : 0.0;
    const Point2f eye_mid{0.5f * (L[33].x + L[263].x), 0.5f * (L[33].y + L[263].y)};
    const double de = dist(L[1], eye_mid), dc = dist(L[152], eye_mid);
    face_.pitch = dc > 0 ? de / dc - 0.5 : 0.0;
    return face_;
}

}  // namespace rppg::vision
