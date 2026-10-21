// Shared helpers of the desktop tools (OpenCV video I/O, argument parsing, JSON output).
#pragma once

#include <cstdio>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include "rppg/engine.hpp"

namespace app {

struct Args {
    std::map<std::string, std::string> kv;
    std::vector<std::string> positional;
    Args(int argc, char** argv) {
        for (int i = 1; i < argc; ++i) {
            std::string a = argv[i];
            if (a.rfind("--", 0) == 0) {
                const auto eq = a.find('=');
                if (eq != std::string::npos)
                    kv[a.substr(2, eq - 2)] = a.substr(eq + 1);
                else if (i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0)
                    kv[a.substr(2)] = argv[++i];
                else
                    kv[a.substr(2)] = "1";
            } else {
                positional.push_back(a);
            }
        }
    }
    bool has(const std::string& k) const { return kv.count(k) > 0; }
    std::string str(const std::string& k, const std::string& d = "") const {
        auto it = kv.find(k);
        return it == kv.end() ? d : it->second;
    }
    double num(const std::string& k, double d) const {
        auto it = kv.find(k);
        return it == kv.end() ? d : std::stod(it->second);
    }
};

inline std::vector<std::string> split(const std::string& s, char sep = ',') {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, sep))
        if (!item.empty()) out.push_back(item);
    return out;
}

// Applies common command-line options to an engine configuration.
inline void apply_config(const Args& a, rppg::EngineConfig& cfg) {
    cfg.model_dir = a.str("models", cfg.model_dir);
    cfg.threads = static_cast<int>(a.num("threads", cfg.threads));
    if (a.has("methods")) {
        cfg.methods.clear();
        for (const auto& m : split(a.str("methods"))) {
            if (m == "none") continue;
            rppg::Method mm;
            if (rppg::method_from_name(m, mm)) cfg.methods.push_back(mm);
            else std::fprintf(stderr, "unknown method %s\n", m.c_str());
        }
    }
    if (a.has("neural")) {
        cfg.neural.clear();
        for (const auto& m : split(a.str("neural"))) {
            if (m == "facephys") cfg.neural.push_back(rppg::NeuralModel::FacePhys);
            else if (m == "me" || m == "me-rppg") cfg.neural.push_back(rppg::NeuralModel::MeRppg);
        }
    }
    if (a.has("multi-roi")) cfg.multi_roi = a.num("multi-roi", 1) != 0;
    cfg.window_s = a.num("window", cfg.window_s);
    cfg.hop_s = a.num("hop", cfg.hop_s);
    cfg.hr_min_bpm = a.num("hr-min", cfg.hr_min_bpm);
    cfg.hr_max_bpm = a.num("hr-max", cfg.hr_max_bpm);
    cfg.fuse = a.num("fuse", cfg.fuse ? 1 : 0) != 0;
    cfg.classical_prior = a.num("classical-prior", cfg.classical_prior);
    cfg.fusion_power = a.num("fusion-power", cfg.fusion_power);
    cfg.pose_aware_prior = a.num("pose-prior", cfg.pose_aware_prior ? 1 : 0) != 0;
    cfg.pose_asym_ref = a.num("pose-ref", cfg.pose_asym_ref);
    cfg.motion_prior_ref = a.num("motion-prior-ref", cfg.motion_prior_ref);
    cfg.track = a.num("track", cfg.track ? 1 : 0) != 0;
    cfg.motion_compensation = a.num("motion-comp", cfg.motion_compensation ? 1 : 0) != 0;
    cfg.track_sigma_bpm_per_s = a.num("track-sigma", cfg.track_sigma_bpm_per_s);
    cfg.max_side = static_cast<int>(a.num("max-side", cfg.max_side));
    cfg.smooth_min_cutoff = a.num("smooth-cutoff", cfg.smooth_min_cutoff);
    cfg.smooth_beta = a.num("smooth-beta", cfg.smooth_beta);
}

inline rppg::ImageView view_of(const cv::Mat& bgr) {
    rppg::ImageView v;
    v.data = bgr.data;
    v.width = bgr.cols;
    v.height = bgr.rows;
    v.stride = static_cast<int>(bgr.step[0]);
    v.format = rppg::PixelFormat::BGR8;
    return v;
}

inline std::string json_escape(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '"' || c == '\\') o.push_back('\\');
        o.push_back(c);
    }
    return o;
}

template <class T>
std::string json_array(const std::vector<T>& v, int precision = 4) {
    std::ostringstream o;
    o.precision(precision);
    o << std::fixed << '[';
    for (std::size_t i = 0; i < v.size(); ++i) o << (i ? "," : "") << v[i];
    o << ']';
    return o.str();
}

}  // namespace app
