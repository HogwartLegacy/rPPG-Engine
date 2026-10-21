# Mobile integration

The engine is plain C++17 + ONNX Runtime; mobile apps call it through the C API
(`include/rppg/c_api.h`). Camera frames are passed zero-copy as NV21/NV12/RGBA/BGRA with their
rotation and presentation timestamp (seconds). Use the camera's own timestamps: the engine resamples
on timestamps, so variable frame rates are handled correctly.

## Android (`bindings/android`)

1. Download `onnxruntime-android-<ver>.aar` (Maven Central `com.microsoft.onnxruntime:onnxruntime-android`)
   and unzip it; it contains `headers/` and `jni/<abi>/libonnxruntime.so`.
2. In the app module:
   ```gradle
   android {
     defaultConfig { externalNativeBuild { cmake {
         arguments "-DRPPG_ONNXRUNTIME_DIR=${rootDir}/third_party/onnxruntime-android"
         abiFilters "arm64-v8a", "armeabi-v7a" } } }
     externalNativeBuild { cmake { path "../rPPG/bindings/android/CMakeLists.txt" } }
   }
   dependencies { implementation "com.microsoft.onnxruntime:onnxruntime-android:1.20.0" }  // ships libonnxruntime.so
   ```
3. Copy `models/` into the APK assets, extract them to `filesDir` once, and create
   `RppgEngine(modelDir = File(filesDir, "models").path)`.
4. CameraX `ImageAnalysis` with `OUTPUT_IMAGE_FORMAT_YUV_420_888`: pass plane 0 (Y) and plane 2
   (V, interleaved VU when `pixelStride == 2`) to `processNv21`, with
   `imageInfo.rotationDegrees` and `imageInfo.timestamp / 1e9`. Keep the analysis at 30 fps and
   640×480; lock auto-exposure / white balance during a measurement when the device allows it.

## iOS (`bindings/ios`)

1. Build the static library:
   `cmake -S . -B build-ios -G Xcode -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_ARCHITECTURES=arm64 -DRPPG_BUILD_APPS=OFF -DRPPG_BUILD_TESTS=OFF -DRPPG_ONNXRUNTIME_DIR=<onnxruntime.xcframework/ios-arm64>`
2. Link `librppg.a` and `onnxruntime.xcframework` (CocoaPods `onnxruntime-c`), add
   `#include "rppg/c_api.h"` to the bridging header and the `models/` folder to the bundle.
3. Configure `AVCaptureVideoDataOutput` with `kCVPixelFormatType_32BGRA` (or
   `420YpCbCr8BiPlanarFullRange`) and feed every `CMSampleBuffer` to `RppgEngine.process`.

## Real-time tips

* Run the engine on the camera callback thread or a dedicated serial queue; one call costs
  ~15 ms (lite) to ~19 ms (default) on a desktop core. Keep `threads = 1` (the tensors are tiny,
  extra intra-op threads were slower in our measurements); on slow phones use the lite preset.
* Show the heart rate only when `vitals.valid && vitals.sqi >= 0.5`; display "hold still" when
  `motion > 0.6` and "more light" when `low_light`.
* The FacePhys licence requires on-device processing and explicit user consent before any
  physiological data leaves the device.
