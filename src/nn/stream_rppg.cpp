#include "nn/stream_rppg.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace rppg::nn {

bool load_state_file(const std::string& path, std::vector<std::string>& names, std::vector<std::vector<float>>& data) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    char magic[8];
    f.read(magic, 8);
    if (std::memcmp(magic, "RPPGSTAT", 8) != 0) return false;
    std::uint32_t version = 0, count = 0;
    f.read(reinterpret_cast<char*>(&version), 4);
    f.read(reinterpret_cast<char*>(&count), 4);
    if (version != 1) return false;
    names.clear();
    data.clear();
    for (std::uint32_t i = 0; i < count; ++i) {
        std::uint32_t len = 0, ndim = 0;
        f.read(reinterpret_cast<char*>(&len), 4);
        std::string name(len, '\0');
        f.read(name.data(), len);
        f.read(reinterpret_cast<char*>(&ndim), 4);
        std::size_t n = 1;
        for (std::uint32_t d = 0; d < ndim; ++d) {
            std::int64_t dim = 0;
            f.read(reinterpret_cast<char*>(&dim), 8);
            n *= static_cast<std::size_t>(dim);
        }
        std::vector<float> v(n);
        f.read(reinterpret_cast<char*>(v.data()), static_cast<std::streamsize>(n * sizeof(float)));
        if (!f) return false;
        names.push_back(std::move(name));
        data.push_back(std::move(v));
    }
    return true;
}

StreamRppg::StreamRppg(const std::string& onnx_path, const std::string& state_path, int threads)
    : model_(onnx_path, threads) {
    const auto& in = model_.inputs();
    const auto& out = model_.outputs();
    for (std::size_t i = 0; i < in.size(); ++i) {
        if (img_in_ < 0 && in[i].elements == 36u * 36u * 3u && in[i].shape.size() == 5 && in[i].shape.back() == 3)
            img_in_ = static_cast<int>(i);
        else if (dt_in_ < 0 && in[i].elements == 1)
            dt_in_ = static_cast<int>(i);
        else
            state_in_.push_back(static_cast<int>(i));
    }
    for (std::size_t i = 0; i < out.size(); ++i) {
        if (bvp_out_ < 0 && out[i].elements == 1)
            bvp_out_ = static_cast<int>(i);
        else
            state_out_.push_back(static_cast<int>(i));
    }
    if (img_in_ < 0 || dt_in_ < 0 || bvp_out_ < 0 || state_in_.size() != state_out_.size())
        throw std::runtime_error("unsupported streaming rPPG model signature: " + onnx_path);
    for (std::size_t k = 0; k < state_in_.size(); ++k)
        if (in[state_in_[k]].elements != out[state_out_[k]].elements)
            throw std::runtime_error("state tensor size mismatch in " + onnx_path);

    // Initial state: learned values (temporal-normalisation statistics) from the state file;
    // missing tensors start at zero.
    std::vector<std::string> names;
    std::vector<std::vector<float>> data;
    const bool have = load_state_file(state_path, names, data);
    init_.resize(state_in_.size());
    for (std::size_t k = 0; k < state_in_.size(); ++k) {
        const TensorInfo& ti = in[state_in_[k]];
        init_[k].assign(ti.elements, 0.f);
        if (!have) continue;
        for (std::size_t j = 0; j < names.size(); ++j)
            if (names[j] == ti.name && data[j].size() == ti.elements) init_[k] = data[j];
    }
    bvp_.resize(1);
    reset();
}

void StreamRppg::reset() {
    cur_ = init_;
    next_ = init_;
}

float StreamRppg::step(const float* frame, float dt) {
    const auto& in = model_.inputs();
    const auto& out = model_.outputs();
    std::vector<const float*> ip(in.size(), nullptr);
    std::vector<float*> op(out.size(), nullptr);
    ip[img_in_] = frame;
    ip[dt_in_] = &dt;
    for (std::size_t k = 0; k < state_in_.size(); ++k) {
        ip[state_in_[k]] = cur_[k].data();
        op[state_out_[k]] = next_[k].data();
    }
    op[bvp_out_] = bvp_.data();
    model_.run(ip, op);
    const float y = bvp_[0];
    if (!std::isfinite(y) || std::abs(y) > 1e6f) {
        // A non-finite value would persist in the recurrent state forever: restart the model.
        reset();
        ++numeric_resets_;
        return std::numeric_limits<float>::quiet_NaN();
    }
    cur_.swap(next_);
    return y;
}

}  // namespace rppg::nn
