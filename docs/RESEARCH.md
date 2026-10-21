# Research survey and component selection

The engine was assembled stage by stage. For each pipeline stage we looked at the open-source
implementations in the provided list (plus newer work found by searching), picked the strongest
option that also runs on a phone CPU, and kept it only if it held up on the benchmark
([BENCHMARK.md](BENCHMARK.md)). Code under GPL (e.g. pyVHR) was used only as a reference:
algorithms were re-implemented from the papers.

## Sources inspected in depth

| Repository / paper | What we took from it |
|---|---|
| [ubicomplab/rPPG-Toolbox](https://github.com/ubicomplab/rPPG-Toolbox) (NeurIPS 2023 D&B) | Evaluation protocol (smoothness-priors detrend, Butterworth, periodogram HR, MAE/RMSE/MAPE/ρ/SNR), reference baselines for every unsupervised method |
| [phuselab/pyVHR](https://github.com/phuselab/pyVHR) (PeerJ CS 2022) | MediaPipe-landmark ROIs, holistic vs. patch processing, motion-aware PSD filtering idea, OMIT/LGI/PBV/2SR formulations |
| [shuoli-roy/optimal_roi_rppg](https://github.com/shuoli-roy/optimal_roi_rppg) (MIT) | The 28 anatomical face-mesh ROI polygons; glabella / forehead / malar as preferred regions |
| [KegangWangCCNU/ME-rPPG](https://github.com/KegangWangCCNU/ME-rPPG), [Health-HCI-Group/ME-rPPG-demo](https://github.com/Health-HCI-Group/ME-rPPG-demo) (arXiv 2504.01774) | Streaming state-space rPPG network (ONNX, 36×36 input, recurrent state, `dt` input), crop convention |
| [KegangWangCCNU/FacePhys-Release](https://github.com/KegangWangCCNU/FacePhys-Release) (2025) | FacePhys streaming SSM network (ONNX) — best published cross-dataset accuracy among deployable models |
| [KegangWangCCNU/open-rppg](https://github.com/KegangWangCCNU/open-rppg) | Inference conventions of 8 networks (input /255, 30 fps, BlazeFace crop), autocorrelation SQI, HRV pipeline |
| [KegangWangCCNU/TemporalNormalization](https://github.com/KegangWangCCNU/TemporalNormalization) | Temporal normalisation (built into FacePhys / ME-rPPG; explains their illumination robustness) |
| PRISM, NeurIPS 2025 ([arXiv 2511.21903](https://arxiv.org/abs/2511.21903)) | Training-free method with online selection of detrending strength + colour mixing by a quality objective (implemented; the idea of quality-driven selection generalised into the candidate fusion) |
| [ksyegorov/mcd_rppg](https://github.com/ksyegorov/mcd_rppg), [MCD-rPPG dataset](https://huggingface.co/datasets/kyegorov/mcd_rppg) | Multi-camera benchmark (webcam, FullHD camcorder, phone; rest + post-exercise) |
| [AiPEX-Lab Bounded Kalman filter](https://github.com/AiPEX-Lab/Bounded-Kalman-filter-method-for-motion-robust-non-contact-heart-rate-estimation) (BOE 2018), [eth-siplab/BeliefPPG](https://github.com/eth-siplab/BeliefPPG) (UAI 2023) | Temporal HR tracking instead of independent per-window peaks → our HMM forward filter / Viterbi |
| DistancePPG (Kumar et al., BOE 2015) | Maximal-ratio combination of facial regions weighted by a "goodness" (SNR) metric — improves darker skin tones |
| Face2PPG / OMIT (Álvarez Casado & Bordallo López, JBHI 2023) | OMIT projection; dynamic region selection |
| [HealthSciTech/E2E-PPG](https://github.com/HealthSciTech/E2E-PPG), [pyPPGqa](https://github.com/HealthSciTech/pyPPGqa), Elgendi 2016, Orphanidou 2015 | Signal-quality index design (SNR, template / periodicity, artefact rejection) |
| Karlen et al. 2013 (smart fusion) | Respiration from intensity, amplitude and frequency modulation of the pulse |
| [google-ai-edge/mediapipe](https://github.com/google-ai-edge/mediapipe) (+ ONNX conversion by QtMeshEditor, Apache-2.0) | BlazeFace short-range detector and Face Mesh V2 (478 landmarks), ROI tracking rules |
| bias-eval-rppg, rppg_biases (Nowara et al.), EquiPleth | Skin-tone bias findings → no fixed skin thresholds, MRC weighting, low-reflectance stress test |
| Awesome-rPPG, rPPG-Trends, mintisan/awesome-ppg | Survey of 2023-2026 work (BeatFormer, StreamPPG, rPPG-VQA, PhysMamba, RhythmMamba, FactorizePhys, ...) |

Other listed repositories (Eulerian magnification, fingertip-PPG apps, BP/SpO2 estimators,
anti-spoofing) were reviewed for scope; they address different problems (motion magnification,
contact PPG, regression of other vitals) and did not contribute components.

## Stage-by-stage decisions

| Stage | Options considered | Chosen (implementation) | Why |
|---|---|---|---|
| Face detection | Haar cascade (toolbox default), YOLO5Face, RetinaFace, YuNet, BlazeFace | **MediaPipe BlazeFace short range** (128×128, 0.4 MB ONNX, ~1 ms) with MediaPipe weighted NMS — `src/vision/face.cpp` | Mobile-grade; same detector family used by the ME-rPPG / FacePhys deployments |
| Landmarks & tracking | dlib-68, OpenFace, 3DDFA_V2, face-alignment, Face Mesh v1 | **Face Mesh V2, 478 points** (ONNX, 5.7 ms CPU) with MediaPipe's tracking ROI (1.5× square, eye-line rotation), re-detection on loss | A dense mesh is required for anatomical ROIs that follow the skin under head rotation |
| Landmark smoothing | Kalman on box (open-rppg), MediaPipe velocity filter | **One-Euro filter** in face-normalised units | Removes ROI jitter at rest without lag during motion |
| ROI definition | Bounding box (toolbox), convex hull (pyVHR), patches, forehead/cheek boxes | **28 anatomical polygons** + holistic union without lips / philtrum / marionette folds; region groups for multi-ROI | The ROI study shows forehead/glabella/malar dominate; excluding the mouth area removes speech/beard artefacts |
| Skin segmentation | Fixed RGB thresholds (pyVHR), fixed YCbCr ranges, BiSeNet face parsing | **Adaptive robust YCbCr model**: histogram median/MAD of central regions, slowly updated, specular/saturation rejection | Fixed thresholds exclude dark or very light skin; parsing networks are too heavy for phones |
| Timing | Constant fps assumption | **Timestamp-based resampling** to 30 Hz | Phone cameras have variable frame intervals; neural models get the true `dt` |
| Colour methods | GREEN, ICA, PCA, CHROM, POS, LGI, PBV, OMIT, 2SR, PRISM | **All ten implemented**; POS + multi-ROI POS used by default | POS is the strongest classical method on both datasets; multi-ROI POS is best whole-video on UBFC (0.46 bpm) |
| Region fusion | Median of patches, clustering (pyVHR), MRC (DistancePPG) | **Maximal-ratio combination** with SNR goodness weights and lag/sign alignment | Coherent gain from good regions while down-weighting occluded or shadowed ones |
| Deep models | DeepPhys, TS-CAN, PhysNet, EfficientPhys, PhysFormer, RhythmFormer/Mamba, PhysMamba, FactorizePhys, ME-rPPG, FacePhys | **FacePhys + ME-rPPG** (streaming state-space models, 36×36 input, 3–4 MB, ~5 ms/frame) | Best cross-dataset results (UBFC 0.43 / 0.47, MMPD 5.3 bpm), constant memory, designed for on-device streaming |
| Pre/post-processing | Tarvainen λ=100, Savitzky-Golay, Butterworth | **O(N) banded smoothness-priors detrend** (cut-off 0.45 Hz) + 2nd-order zero-phase Butterworth, verified against scipy to 1e-9 | Standard toolbox protocol, but linear-time for streaming |
| Spectral HR | FFT peak, Welch, STFT | Hann periodogram on a 0.5 bpm grid, parabolic refinement | Best resolution for 10 s windows |
| Candidate fusion | Single method; PRISM's quality objective | **Iterative consensus fusion**: weights = prior × SNR at the fused peak; classical prior 0.5, **pose-aware** (colour candidates fade out on turned faces, measured by the left/right cheek skin-pixel asymmetry) | Neural and colour methods fail in different situations; the fusion beats every single candidate on UBFC and the pose prior removes the side-view penalty on MCD (5.53 → 5.07 bpm) |
| Motion robustness | pyVHR motion PSD filter, Bounded Kalman, motion regression | **Face-locked ROIs / crops + HMM tracking** (motion spectral attenuation implemented but off by default) | Attenuating head-motion frequencies also removes the pulse when HR coincides with motion (and the ballistocardiographic head motion *is* at HR): turning it off improved shake 30 px 2.43 → 1.55 bpm and MCD 5.37 → 4.99 bpm. Tracking gives the largest gain (MCD 7.1 → 4.9 bpm) |
| HR tracking | Kalman (open-rppg, ME demo), Bounded Kalman, belief propagation (BeliefPPG) | **HMM**: online forward filtering (streaming) / Viterbi (offline), quality-tempered emissions, jump probability for recovery | Rejects transient artefact peaks without freezing on wrong values |
| Signal quality | Autocorrelation (open-rppg), SNR (de Haan), template correlation, skewness, learned SQI | **Composite SQI**: SNR logistic × candidate agreement × face presence × motion penalty | Monotone with error: gating SQI ≥ 0.5 halves MCD error at 60 % coverage |
| HRV | heartpy, NeuroKit | Peak detection with HR-adaptive distance, local-median IBI artefact rejection, SDNN / RMSSD / pNN50, Lomb-Scargle LF/HF | Standard Kubios-style pipeline on the fused pulse |
| Respiration | RIIV / RIAV / RIFV, head motion | Prominence-weighted spectral fusion of four modulations | Smart-fusion principle of Karlen et al. |

## What we tried and did not keep

* **PRISM as default** — faithful implementation reaches 0.66 bpm (UBFC, 10 s windows; paper: 0.66)
  but fails on compressed side-view MCD videos (17 bpm) because its single global projection
  cannot adapt; kept as an optional method.
* **Toolbox CHROM** (band-pass inside each 1.6 s window) — the filter transient spans the whole
  window; replaced by full-length filtering + per-window α (UBFC 10 s MAE 0.79 bpm vs. toolbox 4.06
  whole-video).
* **Data-estimated PBV signature** (toolbox) — hijacked by in-band illumination; replaced by the
  fixed physiological signature of the original paper.
* **Equal fusion priors** — slightly worse on MCD; neural candidates are more reliable on
  compressed / side-view video, so colour candidates get prior 0.5.
* **All candidates in the fusion** — weak methods (ICA, PCA, GREEN, CHROM, PBV) add false peaks;
  the default fuses only POS, multi-ROI POS, FacePhys and ME-rPPG.
* **Motion-spectrum attenuation** (pyVHR-style) and a **motion-aware fusion prior** — both measured
  neutral-to-harmful once ROIs follow the face mesh and the HR is tracked; kept as options
  (`motion_compensation`, `motion_prior_ref`).

## Not included (and why)

* **SpO₂ / blood pressure** from RGB video: not reliable enough with consumer cameras; out of scope.
* **Heavier networks** (PhysFormer, RhythmMamba, PhysMamba: 128×128×160-frame chunks): 15–60 ms per
  chunk-frame on CPU and seconds of latency; the streaming SSMs are as accurate cross-dataset.
* **Face parsing networks** (BiSeNet) for skin: 10–50 ms per frame on a phone; the adaptive colour
  model inside the anatomical ROIs achieves the same goal.
