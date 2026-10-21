# Models

| File | Model | Source | Licence |
|---|---|---|---|
| `mediapipe_face_detector.onnx` | BlazeFace short range (128×128, 896 anchors) | Google MediaPipe Face Landmarker, ONNX conversion by [QtMeshEditor](https://huggingface.co/fernandotonon/QtMeshEditor-models) | Apache-2.0 (`mediapipe_NOTICE.txt`) |
| `mediapipe_face_landmarks.onnx` | Face Mesh V2, 478 landmarks (256×256) | same | Apache-2.0 |
| `facephys.onnx`, `facephys.state.bin` | FacePhys streaming SSM rPPG (36×36 input, trained on RLAP) | [KegangWangCCNU/FacePhys-Release](https://github.com/KegangWangCCNU/FacePhys-Release) | MIT + Privacy Protection Addendum (`LICENSE.facephys.txt`): local on-device inference only, explicit consent for physiological data |
| `me_rppg.onnx`, `me_rppg.state.bin` | ME-rPPG streaming rPPG (36×36 input, trained on RLAP) | [KegangWangCCNU/ME-rPPG](https://github.com/KegangWangCCNU/ME-rPPG), [ME-rPPG-demo](https://github.com/Health-HCI-Group/ME-rPPG-demo) | MIT (`LICENSE.me_rppg.txt`) / Apache-2.0 (demo) |

`*.state.bin` hold the learned initial recurrent states (temporal-normalisation statistics); they
were converted from the authors' `state.json` / `state.gz` with `tools/models/convert_states.py`.
Neither network was trained on UBFC-rPPG or MCD-rPPG, so the benchmark is cross-dataset.
