#include "nn/ort_model.hpp"

#include <stdexcept>

#ifdef RPPG_HAVE_ORT
#include <onnxruntime_cxx_api.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#endif

namespace rppg::nn {

#ifdef RPPG_HAVE_ORT

bool onnxruntime_available() { return true; }

namespace {

Ort::Env& env() {
    static Ort::Env e(ORT_LOGGING_LEVEL_ERROR, "rppg");
    return e;
}

#ifdef _WIN32
std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}
#endif

Ort::SessionOptions options(int threads) {
    Ort::SessionOptions so;
    so.SetIntraOpNumThreads(threads > 0 ? threads : 1);
    so.SetInterOpNumThreads(1);
    so.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
    so.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    return so;
}

std::vector<TensorInfo> describe(Ort::Session& s, bool inputs) {
    Ort::AllocatorWithDefaultOptions alloc;
    const std::size_t n = inputs ? s.GetInputCount() : s.GetOutputCount();
    std::vector<TensorInfo> v(n);
    for (std::size_t i = 0; i < n; ++i) {
        auto name = inputs ? s.GetInputNameAllocated(i, alloc) : s.GetOutputNameAllocated(i, alloc);
        v[i].name = name.get();
        auto ti = inputs ? s.GetInputTypeInfo(i) : s.GetOutputTypeInfo(i);
        auto shape = ti.GetTensorTypeAndShapeInfo().GetShape();
        v[i].elements = 1;
        for (auto& d : shape) {
            if (d <= 0) d = 1;
            v[i].elements *= static_cast<std::size_t>(d);
        }
        v[i].shape = shape;
    }
    return v;
}

}  // namespace

struct OrtModel::Impl {
    Ort::Session session;
    Ort::MemoryInfo mem;
    std::vector<TensorInfo> ins, outs;
    std::vector<const char*> in_names, out_names;

    Impl(const std::string& path, int threads)
#ifdef _WIN32
        : session(env(), widen(path).c_str(), options(threads)),
#else
        : session(env(), path.c_str(), options(threads)),
#endif
          mem(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)) {
        ins = describe(session, true);
        outs = describe(session, false);
        for (auto& t : ins) in_names.push_back(t.name.c_str());
        for (auto& t : outs) out_names.push_back(t.name.c_str());
    }
};

OrtModel::OrtModel(const std::string& path, int threads) {
    try {
        impl_ = std::make_unique<Impl>(path, threads);
    } catch (const Ort::Exception& e) {
        throw std::runtime_error("ONNX Runtime failed to load '" + path + "': " + e.what());
    }
}

OrtModel::~OrtModel() = default;
const std::vector<TensorInfo>& OrtModel::inputs() const { return impl_->ins; }
const std::vector<TensorInfo>& OrtModel::outputs() const { return impl_->outs; }

void OrtModel::run(const std::vector<const float*>& in, const std::vector<float*>& out) {
    auto& I = *impl_;
    if (in.size() != I.ins.size() || out.size() != I.outs.size())
        throw std::invalid_argument("OrtModel::run: buffer count mismatch");
    std::vector<Ort::Value> iv, ov;
    iv.reserve(in.size());
    ov.reserve(out.size());
    for (std::size_t i = 0; i < in.size(); ++i)
        iv.push_back(Ort::Value::CreateTensor<float>(I.mem, const_cast<float*>(in[i]), I.ins[i].elements,
                                                     I.ins[i].shape.data(), I.ins[i].shape.size()));
    for (std::size_t i = 0; i < out.size(); ++i)
        ov.push_back(Ort::Value::CreateTensor<float>(I.mem, out[i], I.outs[i].elements, I.outs[i].shape.data(),
                                                     I.outs[i].shape.size()));
    try {
        I.session.Run(Ort::RunOptions{nullptr}, I.in_names.data(), iv.data(), iv.size(), I.out_names.data(),
                      ov.data(), ov.size());
    } catch (const Ort::Exception& e) {
        throw std::runtime_error(std::string("ONNX Runtime inference failed: ") + e.what());
    }
}

#else  // !RPPG_HAVE_ORT

bool onnxruntime_available() { return false; }
struct OrtModel::Impl {};
OrtModel::OrtModel(const std::string& path, int) {
    throw std::runtime_error("rppg was built without ONNX Runtime; cannot load " + path);
}
OrtModel::~OrtModel() = default;
const std::vector<TensorInfo>& OrtModel::inputs() const {
    static std::vector<TensorInfo> v;
    return v;
}
const std::vector<TensorInfo>& OrtModel::outputs() const { return inputs(); }
void OrtModel::run(const std::vector<const float*>&, const std::vector<float*>&) {}

#endif

}  // namespace rppg::nn
