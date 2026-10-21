package com.rppg

import java.io.Closeable
import java.nio.ByteBuffer

/**
 * Kotlin wrapper of the native rPPG engine.
 *
 * ```
 * val engine = RppgEngine(modelDir = copyAssetsToFiles(context, "models"))
 * imageAnalysis.setAnalyzer(executor) { img ->
 *     val y = img.planes[0]; val uv = img.planes[2]        // NV21 layout: V plane starts the VU data
 *     val r = engine.processNv21(y.buffer, y.rowStride, uv.buffer, uv.rowStride,
 *                                img.width, img.height, img.imageInfo.rotationDegrees,
 *                                img.imageInfo.timestamp / 1e9)
 *     if (r.newVitals) engine.vitals().takeIf { it.valid }?.let { show(it.hrBpm, it.sqi) }
 *     img.close()
 * }
 * ```
 * All processing runs on the device (required by the FacePhys model licence).
 */
class RppgEngine(
    modelDir: String,
    threads: Int = 1,
    neural: Neural = Neural.BOTH,
    classical: Boolean = true,
) : Closeable {
    enum class Neural(val code: Int) { NONE(0), FACEPHYS(1), ME_RPPG(2), BOTH(3) }

    data class FrameResult(val face: Boolean, val x: Float, val y: Float, val w: Float, val h: Float,
                           val neuralBvp: Float, val newVitals: Boolean)

    data class Vitals(
        val valid: Boolean, val hrBpm: Double, val hrRawBpm: Double, val confidence: Double, val sqi: Double,
        val snrDb: Double, val rrBpm: Double, val rrConfidence: Double, val hrvValid: Boolean,
        val sdnnMs: Double, val rmssdMs: Double, val pnn50: Double, val lfHf: Double,
        val faceRatio: Double, val motion: Double, val lowLight: Boolean, val saturation: Boolean,
        val timestamp: Double,
    )

    private var handle: Long = nativeCreate(modelDir, threads, neural.code, classical)

    fun processNv21(y: ByteBuffer, yStride: Int, vu: ByteBuffer, uvStride: Int, width: Int, height: Int,
                    rotation: Int, timestampS: Double): FrameResult =
        toResult(nativeProcessYuv(handle, y, yStride, vu, uvStride, width, height, FORMAT_NV21, rotation, timestampS))

    fun processNv12(y: ByteBuffer, yStride: Int, uv: ByteBuffer, uvStride: Int, width: Int, height: Int,
                    rotation: Int, timestampS: Double): FrameResult =
        toResult(nativeProcessYuv(handle, y, yStride, uv, uvStride, width, height, FORMAT_NV12, rotation, timestampS))

    fun processRgba(rgba: ByteBuffer, stride: Int, width: Int, height: Int, rotation: Int, timestampS: Double): FrameResult =
        toResult(nativeProcessRgba(handle, rgba, stride, width, height, rotation, timestampS))

    fun vitals(): Vitals {
        val a = nativeVitals(handle)
        return Vitals(a[0] > 0, a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8] > 0, a[9], a[10], a[11], a[12],
                      a[13], a[14], a[15] > 0, a[16] > 0, a[17])
    }

    /** Latest fused pulse waveform (z-scored, 30 Hz). */
    fun bvp(): FloatArray = nativeBvp(handle)

    fun reset() = nativeReset(handle)

    override fun close() {
        if (handle != 0L) nativeDestroy(handle)
        handle = 0L
    }

    private fun toResult(r: FloatArray) = FrameResult(r[0] > 0, r[1], r[2], r[3], r[4], r[5], r[6] > 0)

    companion object {
        private const val FORMAT_NV21 = 4
        private const val FORMAT_NV12 = 5

        init { System.loadLibrary("rppg_jni") }

        @JvmStatic external fun nativeVersion(): String
        @JvmStatic private external fun nativeCreate(modelDir: String, threads: Int, neural: Int, classical: Boolean): Long
        @JvmStatic private external fun nativeDestroy(h: Long)
        @JvmStatic private external fun nativeReset(h: Long)
        @JvmStatic private external fun nativeProcessYuv(h: Long, y: ByteBuffer, yStride: Int, uv: ByteBuffer, uvStride: Int,
                                                         width: Int, height: Int, format: Int, rotation: Int, ts: Double): FloatArray
        @JvmStatic private external fun nativeProcessRgba(h: Long, rgba: ByteBuffer, stride: Int, width: Int, height: Int,
                                                          rotation: Int, ts: Double): FloatArray
        @JvmStatic private external fun nativeVitals(h: Long): DoubleArray
        @JvmStatic private external fun nativeBvp(h: Long): FloatArray
    }
}
