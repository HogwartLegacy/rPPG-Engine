// JNI bridge for Android (Kotlin class com.rppg.RppgEngine). Build with the NDK together with
// librppg and the ONNX Runtime Android package (see bindings/android/README.md).
#include <jni.h>

#include <string>
#include <vector>

#include "rppg/c_api.h"

namespace {
rppg_engine* handle(jlong h) { return reinterpret_cast<rppg_engine*>(h); }
}  // namespace

extern "C" {

JNIEXPORT jlong JNICALL Java_com_rppg_RppgEngine_nativeCreate(JNIEnv* env, jclass, jstring model_dir, jint threads,
                                                              jint neural, jboolean classical) {
    rppg_config cfg;
    rppg_default_config(&cfg);
    const char* dir = env->GetStringUTFChars(model_dir, nullptr);
    cfg.model_dir = dir;
    cfg.threads = threads;
    cfg.neural_model = neural;
    cfg.classical = classical ? 1 : 0;
    cfg.max_side = 640;  // camera frames are box-downscaled to <= 640 px for speed
    char err[512] = {0};
    rppg_engine* e = rppg_create(&cfg, err, sizeof(err));
    env->ReleaseStringUTFChars(model_dir, dir);
    if (!e) {
        jclass ex = env->FindClass("java/lang/IllegalStateException");
        env->ThrowNew(ex, err);
        return 0;
    }
    return reinterpret_cast<jlong>(e);
}

JNIEXPORT void JNICALL Java_com_rppg_RppgEngine_nativeDestroy(JNIEnv*, jclass, jlong h) { rppg_destroy(handle(h)); }

JNIEXPORT void JNICALL Java_com_rppg_RppgEngine_nativeReset(JNIEnv*, jclass, jlong h) { rppg_reset(handle(h)); }

// Processes an NV21 / NV12 frame given as direct ByteBuffers (ImageProxy planes). Returns
// [face, x, y, w, h, neuralBvp, newVitals].
JNIEXPORT jfloatArray JNICALL Java_com_rppg_RppgEngine_nativeProcessYuv(JNIEnv* env, jclass, jlong h, jobject y,
                                                                        jint y_stride, jobject uv, jint uv_stride,
                                                                        jint width, jint height, jint format,
                                                                        jint rotation, jdouble timestamp) {
    const auto* py = static_cast<const uint8_t*>(env->GetDirectBufferAddress(y));
    const auto* puv = static_cast<const uint8_t*>(env->GetDirectBufferAddress(uv));
    rppg_frame_result r{};
    const int rc = rppg_process(handle(h), py, width, height, y_stride, format, puv, uv_stride, rotation, timestamp, &r);
    jfloatArray out = env->NewFloatArray(7);
    const float v[7] = {rc == 0 ? static_cast<float>(r.face) : -1.f, r.x, r.y, r.w, r.h, r.neural_bvp,
                        static_cast<float>(r.new_vitals)};
    env->SetFloatArrayRegion(out, 0, 7, v);
    return out;
}

// Processes an RGBA frame (e.g. from a Bitmap / RGBA_8888 ImageReader).
JNIEXPORT jfloatArray JNICALL Java_com_rppg_RppgEngine_nativeProcessRgba(JNIEnv* env, jclass, jlong h, jobject rgba,
                                                                         jint stride, jint width, jint height,
                                                                         jint rotation, jdouble timestamp) {
    const auto* p = static_cast<const uint8_t*>(env->GetDirectBufferAddress(rgba));
    rppg_frame_result r{};
    const int rc = rppg_process(handle(h), p, width, height, stride, RPPG_RGBA8, nullptr, 0, rotation, timestamp, &r);
    jfloatArray out = env->NewFloatArray(7);
    const float v[7] = {rc == 0 ? static_cast<float>(r.face) : -1.f, r.x, r.y, r.w, r.h, r.neural_bvp,
                        static_cast<float>(r.new_vitals)};
    env->SetFloatArrayRegion(out, 0, 7, v);
    return out;
}

// Returns [valid, hr, hrRaw, confidence, sqi, snrDb, rr, rrConfidence, hrvValid, sdnn, rmssd, pnn50, lfHf,
//          faceRatio, motion, lowLight, saturation, timestamp].
JNIEXPORT jdoubleArray JNICALL Java_com_rppg_RppgEngine_nativeVitals(JNIEnv* env, jclass, jlong h) {
    rppg_vitals v{};
    rppg_get_vitals(handle(h), &v);
    const double a[18] = {static_cast<double>(v.valid), v.hr_bpm, v.hr_raw_bpm, v.confidence, v.sqi, v.snr_db,
                          v.rr_bpm, v.rr_confidence, static_cast<double>(v.hrv_valid), v.sdnn_ms, v.rmssd_ms, v.pnn50,
                          v.lf_hf, v.face_ratio, v.motion, static_cast<double>(v.low_light),
                          static_cast<double>(v.saturation), v.timestamp};
    jdoubleArray out = env->NewDoubleArray(18);
    env->SetDoubleArrayRegion(out, 0, 18, a);
    return out;
}

JNIEXPORT jfloatArray JNICALL Java_com_rppg_RppgEngine_nativeBvp(JNIEnv* env, jclass, jlong h) {
    std::vector<float> buf(1024);
    double fs = 0;
    const int n = rppg_get_bvp(handle(h), buf.data(), static_cast<int>(buf.size()), &fs);
    jfloatArray out = env->NewFloatArray(n);
    if (n > 0) env->SetFloatArrayRegion(out, 0, n, buf.data());
    return out;
}

JNIEXPORT jstring JNICALL Java_com_rppg_RppgEngine_nativeVersion(JNIEnv* env, jclass) {
    return env->NewStringUTF(rppg_version());
}

}  // extern "C"
