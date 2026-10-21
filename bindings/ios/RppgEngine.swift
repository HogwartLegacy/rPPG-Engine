// Swift wrapper of the rPPG C API. Add include/rppg/c_api.h to the bridging header
// (#include "rppg/c_api.h") and link librppg.a + onnxruntime.xcframework.
//
//   let engine = try RppgEngine(modelDir: Bundle.main.resourcePath! + "/models")
//   func captureOutput(_ output: AVCaptureOutput, didOutput sb: CMSampleBuffer, from c: AVCaptureConnection) {
//       guard let pb = CMSampleBufferGetImageBuffer(sb) else { return }
//       let t = CMTimeGetSeconds(CMSampleBufferGetPresentationTimeStamp(sb))
//       if engine.process(pixelBuffer: pb, timestamp: t).newVitals, let v = engine.vitals(), v.valid { ... }
//   }
import CoreVideo
import Foundation

public final class RppgEngine {
    public enum Neural: Int32 { case none = 0, facePhys = 1, meRppg = 2, both = 3 }

    public struct Vitals {
        public let valid: Bool
        public let hrBpm, confidence, sqi, snrDb, rrBpm: Double
        public let hrvValid: Bool
        public let sdnnMs, rmssdMs: Double
        public let motion: Double
        public let lowLight: Bool
    }

    private var handle: OpaquePointer?

    public init(modelDir: String, threads: Int32 = 1, neural: Neural = .both, classical: Bool = true) throws {
        var cfg = rppg_config()
        rppg_default_config(&cfg)
        cfg.threads = threads
        cfg.neural_model = neural.rawValue
        cfg.classical = classical ? 1 : 0
        cfg.max_side = 640
        var err = [CChar](repeating: 0, count: 512)
        handle = modelDir.withCString { dir -> OpaquePointer? in
            cfg.model_dir = dir
            return rppg_create(&cfg, &err, Int32(err.count))
        }
        if handle == nil { throw NSError(domain: "rppg", code: 1, userInfo: [NSLocalizedDescriptionKey: String(cString: err)]) }
    }

    deinit { rppg_destroy(handle) }

    /// Accepts kCVPixelFormatType_32BGRA or 420YpCbCr8BiPlanarFullRange buffers.
    @discardableResult
    public func process(pixelBuffer pb: CVPixelBuffer, timestamp: Double, rotation: Int32 = 0) -> (face: Bool, newVitals: Bool) {
        CVPixelBufferLockBaseAddress(pb, .readOnly)
        defer { CVPixelBufferUnlockBaseAddress(pb, .readOnly) }
        var r = rppg_frame_result()
        let w = Int32(CVPixelBufferGetWidth(pb)), h = Int32(CVPixelBufferGetHeight(pb))
        if CVPixelBufferGetPixelFormatType(pb) == kCVPixelFormatType_32BGRA {
            let base = CVPixelBufferGetBaseAddress(pb)!.assumingMemoryBound(to: UInt8.self)
            rppg_process(handle, base, w, h, Int32(CVPixelBufferGetBytesPerRow(pb)), Int32(RPPG_BGRA8), nil, 0, rotation, timestamp, &r)
        } else {
            let y = CVPixelBufferGetBaseAddressOfPlane(pb, 0)!.assumingMemoryBound(to: UInt8.self)
            let uv = CVPixelBufferGetBaseAddressOfPlane(pb, 1)!.assumingMemoryBound(to: UInt8.self)
            rppg_process(handle, y, w, h, Int32(CVPixelBufferGetBytesPerRowOfPlane(pb, 0)), Int32(RPPG_NV12),
                         uv, Int32(CVPixelBufferGetBytesPerRowOfPlane(pb, 1)), rotation, timestamp, &r)
        }
        return (r.face != 0, r.new_vitals != 0)
    }

    public func vitals() -> Vitals? {
        var v = rppg_vitals()
        guard rppg_get_vitals(handle, &v) == 0 else { return nil }
        return Vitals(valid: v.valid != 0, hrBpm: v.hr_bpm, confidence: v.confidence, sqi: v.sqi, snrDb: v.snr_db,
                      rrBpm: v.rr_bpm, hrvValid: v.hrv_valid != 0, sdnnMs: v.sdnn_ms, rmssdMs: v.rmssd_ms,
                      motion: v.motion, lowLight: v.low_light != 0)
    }

    public func bvp() -> [Float] {
        var buf = [Float](repeating: 0, count: 1024)
        var fs = 0.0
        let n = rppg_get_bvp(handle, &buf, Int32(buf.count), &fs)
        return Array(buf.prefix(Int(n)))
    }
}
