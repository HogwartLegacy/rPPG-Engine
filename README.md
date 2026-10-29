# rPPG Engine — camera-based heart rate, pulse waveform, HRV and respiration

A portable C++17 engine that extracts the blood volume pulse (BVP) from facial video
(webcam or smartphone) and turns it into heart rate, signal quality, pulse-rate variability and
respiration rate — in real time, on the CPU, fully on-device.

The engine combines the strongest component of each stage of the open-source / published rPPG
work (see [docs/RESEARCH.md](docs/RESEARCH.md)): MediaPipe face mesh tracking, 28 anatomical skin
ROIs with an adaptive skin model, all major unsupervised colour methods (POS, CHROM, PRISM, OMIT,
LGI, PBV, 2SR, ICA, PCA, GREEN), two state-of-the-art streaming neural models (FacePhys and ME-rPPG),
pose-aware quality-weighted fusion, HMM heart-rate tracking and a signal-quality index.

**Benchmark** (reference: contact PPG of medical oximeters; details in [docs/BENCHMARK.md](docs/BENCHMARK.md)):

| Dataset | Real-time output (causal, 10 s window) | 10 s windows (offline) | Published reference |
|---|---|---|---|
| UBFC-rPPG (38 subjects, webcam) | **MAE 0.62 bpm**, 98.5 % within ±5 bpm | **0.57 bpm** | POS 4.08 / CHROM 4.06 (rPPG-Toolbox, whole video), PRISM 0.66, FacePhys 0.43 |
| MCD-rPPG frontal FullHD camera | **2.04 bpm** | 1.78 bpm | — |
| MCD-rPPG all cameras incl. ~70° side views (44 videos) | 4.99 bpm (2.24 bpm at SQI ≥ 0.5, 61 % coverage) | 4.93 bpm | POS 3.80 (dataset paper, different protocol) |
| Robustness (UBFC, real-time) | H.264 179 kbps 1.84 · camera shake 30 px 1.55 · low light 0.98 · darker-skin proxy 0.73 · in-band flicker 0.73 bpm | | |

Speed: 52 fps (19 ms/frame) on one desktop CPU core for face mesh + both networks + analysis
(i7-12700, 640×480, incl. video decoding); 68 fps for the lite preset.


## Pipeline

```
camera frame (RGB/BGR/RGBA/BGRA/NV12/NV21, any rotation, timestamp)
  │
  ├─ BlazeFace detector (on loss) ─► Face Mesh V2 (478 pts, tracked ROI) ─► One-Euro smoothing
  │
  ├─ 28 anatomical ROIs + holistic skin union ─► adaptive YCbCr skin model ─► per-region RGB means,
  │                                               2nd moments (2SR), background, pose
  ├─ face crop 36×36 ─► FacePhys / ME-rPPG streaming state-space nets (ONNX Runtime) ─► BVP sample
  │
  ▼  every hop (1 s), trailing window (10 s), timestamp-resampled to 30 Hz
candidates: POS (holistic), POS-MR (forehead / cheeks / nose, maximal-ratio combined), FacePhys, ME-rPPG
  │  detrend (smoothness priors) + Butterworth band-pass
  ├─ consensus fusion: weight = prior × SNR at fused peak (pose-aware prior for colour methods)
  ├─ HMM tracking (forward filter live, Viterbi offline)
  └─ SQI, respiration (4 modulations), HRV (beats, IBI cleaning, SDNN/RMSSD/pNN50/LF-HF)
```

## Repository layout

| Path | Content |
|---|---|
| `include/rppg/` | Public API: `engine.hpp` (C++), `c_api.h` (C, for FFI), `methods.hpp`, `dsp.hpp`, `types.hpp` |
| `src/dsp/` | FFT, Butterworth SOS + zero-phase filtering, O(N) smoothness-priors detrend, spectra, splines, peaks (validated against scipy) |
| `src/methods/` | GREEN, ICA (JADE), PCA, CHROM, POS, LGI, PBV, OMIT, 2SR, PRISM |
| `src/vision/` | Image conversion (incl. YUV, rotation), BlazeFace, Face Mesh, tracker, ROIs + skin model |
| `src/nn/` | ONNX Runtime wrapper, streaming neural rPPG with recurrent state |
| `src/engine/` | Engine, analysis / fusion / tracking / SQI, HRV & respiration, C API |
| `apps/` | `rppg_cli` (video file), `rppg_live` (webcam UI), `rppg_bench` (dataset benchmark) |
| `bindings/` | Android (JNI + Kotlin) and iOS (Swift) wrappers |
| `models/` | ONNX models + initial states + licences |
| `tools/` | Dataset download, benchmark sweeps / reports, model state conversion |
| `docs/` | Research survey, benchmark report and raw results |

## Build

Requirements: CMake ≥ 3.20, a C++17 compiler, [ONNX Runtime](https://github.com/microsoft/onnxruntime/releases)
(1.20+, CPU package) for the neural parts, OpenCV (only for the desktop apps).

**Windows** (MSVC 2022 + Ninja):
```powershell
tools\build.ps1 -OrtDir C:\deps\onnxruntime-win-x64-1.20.1 -OpenCVDir C:\deps\opencv\build -Test
```
**Linux / macOS**:
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DRPPG_ONNXRUNTIME_DIR=/opt/onnxruntime -DOpenCV_DIR=/usr/lib/cmake/opencv4
cmake --build build -j && (cd build && ctest)
```
**Android**: `bindings/android/CMakeLists.txt` builds `librppg_jni.so` with the NDK against the
`onnxruntime-android` AAR (`-DRPPG_ONNXRUNTIME_DIR=<extracted aar>`); use `RppgEngine.kt`.
**iOS**: configure with `-DCMAKE_SYSTEM_NAME=iOS -DRPPG_BUILD_APPS=OFF` and the
`onnxruntime-c` xcframework; use `bindings/ios/RppgEngine.swift` with `c_api.h` in the bridging header.

Options: `-DRPPG_WITH_ONNXRUNTIME=OFF` builds a model-free core (classical methods; landmarks must
then be supplied by the platform, e.g. ARKit / ML Kit face mesh), `-DRPPG_BUILD_SHARED=ON` builds a
shared library exporting the C API.

## Usage

```bash
rppg_cli --video face.mp4 --models models --out result.json --bvp-csv bvp.csv
rppg_live --camera 0 --models models          # keys: r ROIs, m mesh, c candidates, s save session
```

C++:
```cpp
#include "rppg/engine.hpp"
rppg::EngineConfig cfg;                 // defaults = benchmarked best configuration
cfg.model_dir = "models";
rppg::Engine engine(cfg);
rppg::ImageView v{bgr.data, width, height, stride, rppg::PixelFormat::BGR8};
rppg::FrameInfo fi = engine.process(v, timestamp_s);
if (fi.new_vitals) {
    rppg::VitalSigns s = engine.vitals();
    if (s.valid && s.sqi >= 0.5) show(s.hr_bpm, s.rr_bpm, s.hrv.rmssd_ms, s.bvp);
}
```

C (any FFI): `rppg_default_config`, `rppg_create`, `rppg_process`, `rppg_get_vitals`,
`rppg_get_bvp`, `rppg_destroy` — see `include/rppg/c_api.h`.

**Presets**

| Preset | Config | Per-frame cost | Use |
|---|---|---|---|
| default | POS + POS-MR + FacePhys + ME-rPPG | 19 ms/frame (desktop core) | desktop, flagship phones |
| lite | `neural = {FacePhys}` | 14.7 ms | mid-range phones |
| classical | `neural = {}` | 11.5 ms | lowest power; also works without ONNX Runtime (external landmarks) |

**Quality**: show the heart rate only when `sqi ≥ 0.5` (or grey it out); `motion`, `low_light`,
`saturation` and `face_ratio` explain low quality to the user.

## Benchmark reproduction

```powershell
python tools\datasets\make_download_lists.py --db db.csv --out D:\rppg_data --mcd-subjects 30   # or any subset
rppg_bench capture --dataset ubfc --root D:\rppg_data\ubfc_rppg --cache D:\rppg_cache\ubfc --models models --jobs 10
rppg_bench capture --dataset mcd  --root D:\rppg_data\mcd_rppg  --cache D:\rppg_cache\mcd  --models models --jobs 10
tools\bench\run_final.ps1 ; tools\bench\run_robustness.ps1
python tools\bench\report.py docs\results\raw\ubfc.csv docs\results\raw\mcd.csv --out docs\results
```
`capture` runs the vision front-end once and caches per-frame traces (`.rppgsess`); `eval` re-runs
any analysis configuration on the cache in seconds (whole-video, 10 s windows and real-time replay).

## Models and licences

Engine code: Apache-2.0. Bundled models (see `models/README.md`): MediaPipe face detector / Face Mesh V2
(Apache-2.0), ME-rPPG (MIT / Apache-2.0), FacePhys (MIT + privacy addendum: inference must run on the
local device and physiological data requires explicit consent — the engine is designed that way).
