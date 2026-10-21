// Streaming (state-space) deep rPPG models: ME-rPPG (arXiv:2504.01774) and FacePhys (2025).
// One 36x36 RGB face crop in, one BVP sample out; recurrent state carried between frames.
#pragma once

#include <string>
#include <vector>

#include "nn/ort_model.hpp"

namespace rppg::nn {

class StreamRppg {
public:
    StreamRppg(const std::string& onnx_path, const std::string& state_path, int threads);
    // frame: input_size() x input_size() x 3 floats, RGB in [0, 1]; dt: seconds since last frame.
    float step(const float* frame, float dt);
    void reset();
    int input_size() const { return 36; }
    std::size_t state_tensors() const { return state_in_.size(); }
    std::size_t numeric_resets() const { return numeric_resets_; }

private:
    OrtModel model_;
    int img_in_ = -1, dt_in_ = -1, bvp_out_ = -1;
    std::vector<int> state_in_, state_out_;
    std::vector<std::vector<float>> init_, cur_, next_;
    std::vector<float> bvp_;
    std::size_t numeric_resets_ = 0;
};

// Reads the "RPPGSTAT" binary state file written by tools/models/convert_states.py.
bool load_state_file(const std::string& path, std::vector<std::string>& names,
                     std::vector<std::vector<float>>& data);

}  // namespace rppg::nn
