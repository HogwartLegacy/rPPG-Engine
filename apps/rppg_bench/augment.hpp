// On-the-fly video perturbations for robustness benchmarking (applied before the engine sees a frame).
//
//   dark:k       photon-limited low light: intensity x k, shot noise (Poisson approx.) + read noise,
//                8-bit re-quantisation
//   melanin:l    darker skin-tone / low-reflectance proxy: channel attenuation (R 0.55, G 0.42,
//                B 0.36 at l = 1, melanin absorbs more at short wavelengths) + sensor noise
//   noise:s      additive Gaussian sensor noise (DN)
//   flicker:a    in-band global illumination modulation (1 + a sin), frequency wandering 0.9-2.2 Hz
//   shake:p      camera shake: translation up to p pixels (0.7 / 1.6 Hz components) + rotation
//   lowres:w     down-sample to width w and back (subject far from the camera)
#pragma once

#include <cmath>
#include <random>
#include <string>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

namespace bench {

class Augment {
public:
    Augment() = default;
    explicit Augment(const std::string& spec) {
        const auto c = spec.find(':');
        kind_ = spec.substr(0, c);
        p_ = c == std::string::npos ? 0.0 : std::stod(spec.substr(c + 1));
    }
    bool active() const { return !kind_.empty() && kind_ != "none"; }
    const std::string& kind() const { return kind_; }

    void begin(unsigned seed) {
        rng_.seed(seed);
        std::uniform_real_distribution<double> u(0, 2 * M_PI);
        for (double& ph : phase_) ph = u(rng_);
        flick_phase_ = 0;
        flick_f_ = 1.4;
        last_t_ = 0;
    }

    void apply(cv::Mat& bgr, double t) {
        if (!active()) return;
        if (kind_ == "dark") {
            photon(bgr, cv::Scalar(p_, p_, p_));
        } else if (kind_ == "melanin") {
            const double l = std::clamp(p_, 0.0, 1.0);
            photon(bgr, cv::Scalar(1 - l * (1 - 0.36), 1 - l * (1 - 0.42), 1 - l * (1 - 0.55)));  // B, G, R
        } else if (kind_ == "noise") {
            cv::Mat f;
            bgr.convertTo(f, CV_32FC3);
            cv::Mat n(f.size(), CV_32FC3);
            cv::randn(n, 0.0, p_);
            f += n;
            f.convertTo(bgr, CV_8UC3);
        } else if (kind_ == "flicker") {
            const double dt = std::max(0.0, t - last_t_);
            last_t_ = t;
            std::normal_distribution<double> nd(0, 0.35);
            flick_f_ = std::clamp(flick_f_ + nd(rng_) * std::sqrt(dt), 0.9, 2.2);
            flick_phase_ += 2 * M_PI * flick_f_ * dt;
            const double g = (1 + p_ * std::sin(flick_phase_)) * (1 + 0.08 * std::sin(2 * M_PI * 0.05 * t + phase_[0]));
            bgr.convertTo(bgr, CV_8UC3, g);
        } else if (kind_ == "shake") {
            const double tx = p_ * (0.6 * std::sin(2 * M_PI * 0.7 * t + phase_[0]) + 0.4 * std::sin(2 * M_PI * 1.6 * t + phase_[1]));
            const double ty = p_ * (0.6 * std::sin(2 * M_PI * 0.5 * t + phase_[2]) + 0.4 * std::sin(2 * M_PI * 1.3 * t + phase_[3]));
            const double ang = (p_ / 50.0) * 3.0 * std::sin(2 * M_PI * 0.9 * t + phase_[4]);
            cv::Mat M = cv::getRotationMatrix2D(cv::Point2f(bgr.cols / 2.f, bgr.rows / 2.f), ang, 1.0);
            M.at<double>(0, 2) += tx;
            M.at<double>(1, 2) += ty;
            cv::Mat out;
            cv::warpAffine(bgr, out, M, bgr.size(), cv::INTER_LINEAR, cv::BORDER_REPLICATE);
            bgr = out;
        } else if (kind_ == "lowres") {
            const int w = std::max(16, static_cast<int>(p_));
            const int h = std::max(16, static_cast<int>(std::lround(bgr.rows * static_cast<double>(w) / bgr.cols)));
            cv::Mat small, up;
            cv::resize(bgr, small, cv::Size(w, h), 0, 0, cv::INTER_AREA);
            cv::resize(small, up, bgr.size(), 0, 0, cv::INTER_LINEAR);
            bgr = up;
        }
    }

private:
    // Scales the photon flux per channel and adds shot (variance ~ signal) and read noise.
    void photon(cv::Mat& bgr, const cv::Scalar& k) {
        cv::Mat f;
        bgr.convertTo(f, CV_32FC3);
        cv::multiply(f, k, f);
        cv::Mat sd = f * 0.6;  // shot noise: sigma^2 = gain * signal (gain 0.6 DN/e-)
        cv::max(sd, 0.0, sd);
        cv::sqrt(sd, sd);
        cv::Mat n(f.size(), CV_32FC3);
        cv::randn(n, 0.0, 1.0);
        f += n.mul(sd);
        cv::randn(n, 0.0, 1.2);  // read noise
        f += n;
        f.convertTo(bgr, CV_8UC3);
    }

    std::string kind_;
    double p_ = 0;
    std::mt19937 rng_;
    double phase_[5] = {0, 0, 0, 0, 0};
    double flick_phase_ = 0, flick_f_ = 1.4, last_t_ = 0;
};

}  // namespace bench
