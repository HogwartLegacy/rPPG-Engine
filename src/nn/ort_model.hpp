// Thin ONNX Runtime wrapper: float tensors bound to caller-owned buffers (no copies).
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace rppg::nn {

struct TensorInfo {
    std::string name;
    std::vector<std::int64_t> shape;  // dynamic dimensions replaced by 1
    std::size_t elements = 1;
};

// True when the library was built with ONNX Runtime.
bool onnxruntime_available();

class OrtModel {
public:
    OrtModel(const std::string& path, int threads = 1);
    ~OrtModel();
    OrtModel(const OrtModel&) = delete;
    OrtModel& operator=(const OrtModel&) = delete;

    const std::vector<TensorInfo>& inputs() const;
    const std::vector<TensorInfo>& outputs() const;
    // Buffers follow the declared order and shapes of inputs() / outputs().
    void run(const std::vector<const float*>& in, const std::vector<float*>& out);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace rppg::nn
