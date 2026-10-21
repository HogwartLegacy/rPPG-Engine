"""Prototype of the MediaPipe face detector + Face Mesh V2 decoding (reference for the C++ port).

Usage: python proto_face.py video.avi out.png
"""
import sys

import cv2
import numpy as np
import onnxruntime as ort

MODELS = __file__.rsplit("tools", 1)[0] + "models/"


def anchors_short_range():
    # MediaPipe SsdAnchorsCalculator, face_detection_short_range: 128x128, strides 8,16,16,16.
    out = []
    for stride, n in ((8, 2), (16, 6)):
        fm = 128 // stride
        for y in range(fm):
            for x in range(fm):
                for _ in range(n):
                    out.append(((x + 0.5) / fm, (y + 0.5) / fm))
    return np.array(out, np.float32)


def detect(sess, img_rgb, norm):
    h, w = img_rgb.shape[:2]
    s = min(128 / w, 128 / h)
    nw, nh = int(round(w * s)), int(round(h * s))
    pad = np.zeros((128, 128, 3), np.uint8)
    ox, oy = (128 - nw) // 2, (128 - nh) // 2
    pad[oy:oy + nh, ox:ox + nw] = cv2.resize(img_rgb, (nw, nh), interpolation=cv2.INTER_LINEAR)
    x = pad.astype(np.float32) / 255.0
    if norm == "pm1":
        x = x * 2 - 1
    reg, cls = sess.run(None, {"input": x[None]})
    reg, cls = reg[0], cls[0, :, 0]
    score = 1 / (1 + np.exp(-np.clip(cls, -100, 100)))
    a = anchors_short_range()
    i = int(np.argmax(score))
    cx = reg[i, 0] / 128 + a[i, 0]
    cy = reg[i, 1] / 128 + a[i, 1]
    bw, bh = reg[i, 2] / 128, reg[i, 3] / 128
    kps = np.stack([reg[i, 4::2] / 128 + a[i, 0], reg[i, 5::2] / 128 + a[i, 1]], 1)
    # undo letterbox
    def unpad(px, py):
        return ((px * 128 - ox) / s, (py * 128 - oy) / s)
    c = unpad(cx, cy)
    k = np.array([unpad(*p) for p in kps])
    return score[i], c, (bw * 128 / s, bh * 128 / s), k


def roi_from_detection(center, size, kps):
    x0, y0 = kps[0]
    x1, y1 = kps[1]
    rot = -np.arctan2(-(y1 - y0), x1 - x0)
    side = max(size) * 1.5
    return center, side, rot


def crop(img, center, side, rot, out=256):
    c, s = np.cos(rot), np.sin(rot)
    # image = center + side * R * ((u+0.5)/out - 0.5)
    M = np.array([[c * side / out, -s * side / out, 0], [s * side / out, c * side / out, 0]], np.float64)
    M[:, 2] = np.array(center) - M[:, :2] @ np.array([out / 2 - 0.5, out / 2 - 0.5]) * 1.0
    # cv2.warpAffine maps dst->src with WARP_INVERSE_MAP
    patch = cv2.warpAffine(img, M, (out, out), flags=cv2.INTER_LINEAR | cv2.WARP_INVERSE_MAP,
                           borderMode=cv2.BORDER_CONSTANT)
    return patch, M


def main(video, out_png):
    cap = cv2.VideoCapture(video)
    for _ in range(60):
        ok, frame = cap.read()
    rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    det = ort.InferenceSession(MODELS + "mediapipe_face_detector.onnx", providers=["CPUExecutionProvider"])
    for norm in ("pm1", "01"):
        sc, c, size, k = detect(det, rgb, norm)
        print(f"norm={norm}: score={sc:.3f} center=({c[0]:.1f},{c[1]:.1f}) size=({size[0]:.1f},{size[1]:.1f})")
    sc, c, size, k = detect(det, rgb, "pm1")
    center, side, rot = roi_from_detection(c, size, k)
    patch, M = crop(rgb, center, side, rot)
    lm = ort.InferenceSession(MODELS + "mediapipe_face_landmarks.onnx", providers=["CPUExecutionProvider"])
    outs = lm.run(None, {lm.get_inputs()[0].name: (patch.astype(np.float32) / 255.0)[None]})
    pts = outs[0].reshape(-1, 3)
    print("landmarks", pts.shape, "presence logit", outs[1].ravel(), "aux", outs[2].ravel())
    img_pts = (M[:, :2] @ pts[:, :2].T).T + M[:, 2]
    vis = frame.copy()
    for (x, y) in img_pts.astype(int):
        cv2.circle(vis, (x, y), 1, (0, 255, 0), -1)
    for (x, y) in k.astype(int):
        cv2.circle(vis, (x, y), 4, (0, 0, 255), -1)
    cv2.imwrite(out_png, vis)
    print("eye corners 33/263:", img_pts[33], img_pts[263], "forehead 10:", img_pts[10], "chin 152:", img_pts[152])


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
