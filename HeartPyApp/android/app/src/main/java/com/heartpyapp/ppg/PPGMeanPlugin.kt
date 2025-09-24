package com.heartpyapp.ppg

import android.media.Image
import android.util.Log
import com.mrousavy.camera.frameprocessors.Frame
import com.mrousavy.camera.frameprocessors.FrameProcessorPlugin
import com.mrousavy.camera.frameprocessors.FrameProcessorPluginRegistry
import java.nio.ByteBuffer
import java.util.Locale

private const val DEFAULT_TARGET_RMS = 0.02
private const val DEFAULT_ALPHA_RMS = 0.05
private const val DEFAULT_ALPHA_GAIN = 0.1
private const val DEFAULT_GAIN_MIN = 0.5
private const val DEFAULT_GAIN_MAX = 20.0
private const val MIN_RMS_FRACTION = 0.1
import com.heartpy.HeartPyModule

/**
 * Simple ROI mean-intensity plugin.
 *
 * Android fast-path uses Y (luma) plane from YUV_420 frame. This avoids color conversion.
 * Returns a Double (mean intensity 0..255) over a centered ROI.
 */
class PPGMeanPlugin : FrameProcessorPlugin() {
  // Rolling history buffers for aggregated ROI means (R,G,B,Y)
  private val HIST_N = 64
  private val rHist = DoubleArray(HIST_N)
  private val gHist = DoubleArray(HIST_N)
  private val bHist = DoubleArray(HIST_N)
  private val yHist = DoubleArray(HIST_N)
  private val vHist = DoubleArray(HIST_N)
  private var dcMean = Double.NaN
  private var signalDc = Double.NaN
  private var agcRms = 0.0
  private var agcGain = 1.0
  private var histPos = 0
  private var histCount = 0
  private var simdEnabledFlag = true
  private var performanceLoggingFlag = false
  private var parityCountdown = PARITY_INTERVAL
  private var parityMaxDiff = 0.0
  private var parityAccumDiff = 0.0
  private var paritySamples = 0
  private val frameTimesMs = DoubleArray(PERF_WINDOW)
  private var frameTimeIndex = 0
  private var frameTimeCount = 0
  private var frameCounter = 0
  override fun callback(frame: Frame, params: Map<String, Any?>?): Any? {
    return try {
      val simdFlagParam = params?.get("simdEnabled") as? Boolean
      if (simdFlagParam != null) {
        simdEnabledFlag = simdFlagParam
      }
      performanceLoggingFlag = (params?.get("performanceLogging") as? Boolean) == true

      val simdActiveThisFrame = simdEnabledFlag && nativeLibLoaded
      if (performanceLoggingFlag && simdActiveThisFrame && parityCountdown > 0) {
        parityCountdown -= 1
      }
      var parityCheckPending = performanceLoggingFlag && simdActiveThisFrame && parityCountdown <= 0
      val perfStartNanos = if (performanceLoggingFlag) System.nanoTime() else 0L

      var roiIn = (params?.get("roi") as? Number)?.toFloat() ?: 0.4f
      val channel = (params?.get("channel") as? String) ?: "green"
      val mode = (params?.get("mode") as? String) ?: "mean" // mean | chrom | pos
      val useCHROM = (mode == "chrom" || mode == "pos")
      val blend = (params?.get("blend") as? String) ?: "off" // off | auto
      val autoBlend = (blend == "auto")
      val torchOnHint = (params?.get("torch") as? Boolean) ?: false
      val gridIn = (params?.get("grid") as? Number)?.toInt() ?: 1
      val grid = gridIn.coerceIn(1, 3)
      val stepIn = (params?.get("step") as? Number)?.toInt() ?: 2
      val step = stepIn.coerceIn(1, 8)
      // Clamp ROI to sane bounds
      var roi = roiIn.coerceIn(0.2f, 0.6f)

      val image = frame.image
      val planes = image.planes
      // Use plane 0 (Y plane) for mean intensity or as Y component for red estimation
      val yPlane = planes[0]
      val yBuffer: ByteBuffer = yPlane.buffer
      val yRowStride = yPlane.rowStride
      val yPixStride = yPlane.pixelStride

      val width = frame.width
      val height = frame.height
      if (width <= 0 || height <= 0) return java.lang.Double.NaN

      var roiW = (width * roi).toInt().coerceAtLeast(1)
      var roiH = (height * roi).toInt().coerceAtLeast(1)

      // Area guard: ensure ROI covers at least 10% of frame
      val minArea = 0.1f * width.toFloat() * height.toFloat()
      if (roiW.toFloat() * roiH.toFloat() < minArea) {
        roi = 0.4f
        roiW = (width * roi).toInt().coerceAtLeast(1)
        roiH = (height * roi).toInt().coerceAtLeast(1)
      }

      val startX = ((width - roiW) / 2).coerceAtLeast(0)
      val startY = ((height - roiH) / 2).coerceAtLeast(0)

      // Sample grid step for speed
      val xStep = step
      val yStep = step

      val useRedOrGreen = (channel == "red" || channel == "green") && planes.size >= 3
      val useSimdPaths = simdActiveThisFrame
      // Multi-ROI aggregation across grid x grid patches
      var weightedSum = 0.0
      var weightTotal = 0.0
      var confAccum = 0.0
      var spatialSum = 0.0
      var spatialSqSum = 0.0
      var spatialSamples = 0.0
      var wSumR = 0.0; var wSumG = 0.0; var wSumB = 0.0; var wSumY = 0.0

      val uPlane = planes.getOrNull(1)
      val vPlane = planes.getOrNull(2)
      val uBuffer: ByteBuffer? = uPlane?.buffer
      val vBuffer: ByteBuffer? = vPlane?.buffer
      val uRowStride = uPlane?.rowStride ?: 0
      val uPixStride = uPlane?.pixelStride ?: 0
      val vRowStride = vPlane?.rowStride ?: 0
      val vPixStride = vPlane?.pixelStride ?: 0

      val patchW = (roiW / grid).coerceAtLeast(1)
      val patchH = (roiH / grid).coerceAtLeast(1)
      for (gy in 0 until grid) {
        for (gx in 0 until grid) {
          val px0 = startX + gx * patchW
          val py0 = startY + gy * patchH
          val px1 = if (gx == grid - 1) startX + roiW else px0 + patchW
          val py1 = if (gy == grid - 1) startY + roiH else py0 + patchH
          val px1c = (startX + roiW).coerceAtMost(px1)
          val sampleCols = computeSampleCount(px1c - px0, xStep)
          val sampleRows = computeSampleCount(py1 - py0, yStep)
          var sampleCount = sampleCols * sampleRows
          val baseOffset = py0 * yRowStride + px0 * yPixStride
          val simdStats = if (useSimdPaths) {
            sumChannelSimd(
              yBuffer,
              baseOffset,
              yRowStride,
              px1c - px0,
              py1 - py0,
              yPixStride,
              0,
              xStep,
              yStep,
            )
          } else {
            null
          }
          if (simdStats != null) {
            sampleCount = simdStats.count
          }
          val doParityForPatch = parityCheckPending && simdStats != null
          var paritySumY = 0.0
          var parityCount = 0
          var sR = 0.0; var sG = 0.0; var sB = 0.0
          var scalarSumY = 0.0; var scalarSumSqY = 0.0; var scalarCount = 0

          for (y in py0 until py1 step yStep) {
            val yRow = y * yRowStride
            val uvY = y shr 1
            val uRow = uvY * uRowStride
            val vRow = uvY * vRowStride
            for (x in px0 until px1c step xStep) {
              val yIdx = yRow + x * yPixStride
              val yVal = (yBuffer.get(yIdx).toInt() and 0xFF).toDouble()
              val uvX = x shr 1
              val uIdx = uRow + uvX * uPixStride
              val vIdx = vRow + uvX * vPixStride
              val Cb = (uBuffer?.get(uIdx)?.toInt() ?: 128) and 0xFF
              val Cr = (vBuffer?.get(vIdx)?.toInt() ?: 128) and 0xFF
              val cb = Cb.toDouble() - 128.0
              val cr = Cr.toDouble() - 128.0
              var R = yVal + 1.402 * cr
              var G = yVal - 0.344 * cb - 0.714 * cr
              var B = yVal + 1.772 * cb
              if (R < 0.0) R = 0.0; if (R > 255.0) R = 255.0
              if (G < 0.0) G = 0.0; if (G > 255.0) G = 255.0
              if (B < 0.0) B = 0.0; if (B > 255.0) B = 255.0
              sR += R; sG += G; sB += B
              if (simdStats == null) {
                scalarSumY += yVal
                scalarSumSqY += yVal * yVal
                scalarCount += 1
              }
              if (doParityForPatch) {
                paritySumY += yVal
                parityCount += 1
              }
            }
          }
          if (simdStats == null) {
            sampleCount = scalarCount
          }
          if (sampleCount <= 0) continue

          val patchSumY = simdStats?.sum ?: scalarSumY
          val patchSumSqY = simdStats?.sumSq ?: scalarSumSqY
          val Ym = patchSumY / sampleCount.toDouble()
          spatialSum += patchSumY
          spatialSqSum += patchSumSqY
          spatialSamples += sampleCount.toDouble()

          if (doParityForPatch && parityCount > 0 && simdStats != null && simdStats.count > 0) {
            val simdMean = simdStats.sum / simdStats.count.toDouble()
            val scalarMean = paritySumY / parityCount.toDouble()
            updateParityStats(kotlin.math.abs(simdMean - scalarMean))
            parityCheckPending = false
            parityCountdown = PARITY_INTERVAL
          }

          val Rm = sR / sampleCount.toDouble()
          val Gm = sG / sampleCount.toDouble()
          val Bm = sB / sampleCount.toDouble()
          var value = when (channel) {
            "red" -> Rm
            "luma" -> Ym
            else -> Gm
          }
          if (value < 0.0) value = 0.0
          if (value > 255.0) value = 255.0
          val expScore = when {
            Ym < 15.0 -> (Ym / 15.0).coerceIn(0.0, 1.0)
            Ym > 240.0 -> ((255.0 - Ym) / 15.0).coerceIn(0.0, 1.0)
            else -> 1.0
          }
          val ampScore = if (useCHROM) {
            val Sabs = kotlin.math.abs((3.0 * Rm - 2.0 * Gm) - (1.5 * Rm + 1.0 * Gm - 1.5 * Bm))
            (Sabs / 50.0).coerceIn(0.0, 1.0)
          } else 0.0
          val conf = (0.7 * expScore + 0.3 * ampScore).coerceIn(0.0, 1.0)
          val w = expScore.coerceAtLeast(1e-6)
          weightedSum += w * value
          weightTotal += w
          confAccum += w * conf
          wSumR += w * Rm; wSumG += w * Gm; wSumB += w * Bm; wSumY += w * Ym
        }
      }
      val resultMean = if (weightTotal > 0.0) weightedSum / weightTotal else java.lang.Double.NaN
      val Ragg = if (weightTotal > 0.0) wSumR / weightTotal else Double.NaN
      val Gagg = if (weightTotal > 0.0) wSumG / weightTotal else Double.NaN
      val Bagg = if (weightTotal > 0.0) wSumB / weightTotal else Double.NaN
      val Yagg = if (weightTotal > 0.0) wSumY / weightTotal else Double.NaN

      val spatialMean = if (spatialSamples > 0.0) spatialSum / spatialSamples else Double.NaN
      val spatialVar = if (spatialSamples > 0.0) (spatialSqSum / spatialSamples) - spatialMean * spatialMean else 0.0
      val spatialStd = if (spatialVar.isNaN() || spatialVar <= 0.0) 0.0 else kotlin.math.sqrt(spatialVar)
      val contrastScore = (spatialStd / 12.0).coerceIn(0.0, 1.0)

      if (Ragg.isFinite() && Gagg.isFinite() && Bagg.isFinite() && Yagg.isFinite()) {
        rHist[histPos] = Ragg; gHist[histPos] = Gagg; bHist[histPos] = Bagg; yHist[histPos] = Yagg; vHist[histPos] = resultMean
        histPos = (histPos + 1) % HIST_N
        if (histCount < HIST_N) histCount++
        if (histCount < 8) signalDc = Double.NaN
      }

      // Compute out sample
      var chromVal = Double.NaN
      var chromAmp = 0.0
      if (histCount >= 8) {
        val N = histCount
        // CHROM rolling alpha over history
        var meanX = 0.0; var meanYc = 0.0
        for (i in 0 until N) {
          val idx = (histPos - 1 - i + HIST_N) % HIST_N
          val X = 3.0 * rHist[idx] - 2.0 * gHist[idx]
          val Yc = 1.5 * rHist[idx] + 1.0 * gHist[idx] - 1.5 * bHist[idx]
          meanX += X; meanYc += Yc
        }
        meanX /= N; meanYc /= N
        var varX = 0.0; var varY = 0.0
        for (i in 0 until N) {
          val idx = (histPos - 1 - i + HIST_N) % HIST_N
          val X = 3.0 * rHist[idx] - 2.0 * gHist[idx]
          val Yc = 1.5 * rHist[idx] + 1.0 * gHist[idx] - 1.5 * bHist[idx]
          varX += (X - meanX) * (X - meanX)
          varY += (Yc - meanYc) * (Yc - meanYc)
        }
        val stdX = kotlin.math.sqrt(varX / kotlin.math.max(1, N - 1).toDouble())
        val stdY = kotlin.math.sqrt(varY / kotlin.math.max(1, N - 1).toDouble())
        chromAmp = stdX
        val alpha = if (stdY > 1e-6) stdX / stdY else 1.0
        val lastIdx = (histPos - 1 + HIST_N) % HIST_N
        val Xcur = 3.0 * rHist[lastIdx] - 2.0 * gHist[lastIdx]
        val Ycur = 1.5 * rHist[lastIdx] + 1.0 * gHist[lastIdx] - 1.5 * bHist[lastIdx]
        val Sc = if (mode == "chrom") {
          Xcur - alpha * Ycur
        } else {
          // POS: normalized RGB, S1_last + alpha_pos * S2_last
          var meanR = 0.0; var meanG = 0.0; var meanB = 0.0
          for (i in 0 until N) {
            val idx = (histPos - 1 - i + HIST_N) % HIST_N
            meanR += rHist[idx]; meanG += gHist[idx]; meanB += bHist[idx]
          }
          meanR /= N; meanG /= N; meanB /= N
          var s1m = 0.0; var s2m = 0.0
          for (i in 0 until N) {
            val idx = (histPos - 1 - i + HIST_N) % HIST_N
            val rN = rHist[idx] / kotlin.math.max(1e-6, meanR) - 1.0
            val gN = gHist[idx] / kotlin.math.max(1e-6, meanG) - 1.0
            val bN = bHist[idx] / kotlin.math.max(1e-6, meanB) - 1.0
            val s1 = 3.0 * rN - 2.0 * gN
            val s2 = 1.5 * rN + 1.0 * gN - 1.5 * bN
            s1m += s1; s2m += s2
          }
          s1m /= N; s2m /= N
          var v1 = 0.0; var v2 = 0.0
          for (i in 0 until N) {
            val idx = (histPos - 1 - i + HIST_N) % HIST_N
            val rN = rHist[idx] / kotlin.math.max(1e-6, meanR) - 1.0
            val gN = gHist[idx] / kotlin.math.max(1e-6, meanG) - 1.0
            val bN = bHist[idx] / kotlin.math.max(1e-6, meanB) - 1.0
            val s1 = 3.0 * rN - 2.0 * gN
            val s2 = 1.5 * rN + 1.0 * gN - 1.5 * bN
            v1 += (s1 - s1m) * (s1 - s1m)
            v2 += (s2 - s2m) * (s2 - s2m)
          }
          val std1 = kotlin.math.sqrt(v1 / kotlin.math.max(1, N - 1).toDouble())
          val std2 = kotlin.math.sqrt(v2 / kotlin.math.max(1, N - 1).toDouble())
          val aPos = if (std2 > 1e-6) std1 / std2 else 1.0
          val rN = rHist[lastIdx] / kotlin.math.max(1e-6, meanR) - 1.0
          val gN = gHist[lastIdx] / kotlin.math.max(1e-6, meanG) - 1.0
          val bN = bHist[lastIdx] / kotlin.math.max(1e-6, meanB) - 1.0
          val s1 = 3.0 * rN - 2.0 * gN
          val s2 = 1.5 * rN + 1.0 * gN - 1.5 * bN
          s1 + aPos * s2
        }
        val k = 0.5
        chromVal = (128.0 + k * Sc).coerceIn(0.0, 255.0)
      }

      var windowMean = resultMean
      var temporalScore = 0.2
      var temporalStd = 0.0
      if (histCount >= 6) {
        val window = kotlin.math.min(histCount, 30)
        var meanHist = 0.0
        for (i in 0 until window) {
          val idx = (histPos - 1 - i + HIST_N) % HIST_N
          meanHist += vHist[idx]
        }
        meanHist /= window.toDouble()
        windowMean = meanHist
        var varHist = 0.0
        for (i in 0 until window) {
          val idx = (histPos - 1 - i + HIST_N) % HIST_N
          val d = vHist[idx] - meanHist
          varHist += d * d
        }
        temporalStd = kotlin.math.sqrt(varHist / kotlin.math.max(1, window - 1).toDouble())
        temporalScore = (temporalStd / 6.0).coerceIn(0.0, 1.0)
      }
      if (!temporalScore.isFinite()) temporalScore = 0.0
      val amplitudeScore = (chromAmp / 35.0).coerceIn(0.0, 1.0)

      // Dynamic exposure gating via percentiles of Y history
      var expoGate = 1.0
      if (histCount >= 16) {
        val N = histCount
        val tmp = DoubleArray(N)
        for (i in 0 until N) {
          val idx = (histPos - 1 - i + HIST_N) % HIST_N
          tmp[i] = yHist[idx]
        }
        java.util.Arrays.sort(tmp)
        val p10 = tmp[(0.1 * (N - 1)).toInt()]
        val p90 = tmp[(0.9 * (N - 1)).toInt()]
        val gDark = (p10 / 20.0).coerceIn(0.0, 1.0)
        val gSat = ((255.0 - p90) / 20.0).coerceIn(0.0, 1.0)
        expoGate = kotlin.math.min(gDark, gSat)
      }
      val expoScore = expoGate.coerceIn(0.0, 1.0)
      val patchScore = if (weightTotal > 0.0) (confAccum / weightTotal).coerceIn(0.0, 1.0) else expoScore
      val spatialGate = (0.6 * expoScore + 0.4 * contrastScore).coerceIn(0.0, 1.0)
      val dynamicMix = (0.7 * temporalScore + 0.3 * amplitudeScore).coerceIn(0.0, 1.0)
      val reliability = kotlin.math.sqrt((spatialGate * dynamicMix).coerceIn(0.0, 1.0))
      val baseConf = (0.3 * patchScore + 0.7 * reliability).coerceIn(0.0, 1.0)
      val confMean = baseConf
      val confChrom = (0.6 * baseConf + 0.4 * dynamicMix).coerceIn(0.0, 1.0)

      // Default by mode
      var outVal = if (mode == "mean" || !chromVal.isFinite()) resultMean else chromVal
      var outConf = if (mode == "mean" || !chromVal.isFinite()) confMean else confChrom
      var blendWeight = 0.0
      var blendUsed = false

      // Auto crossfade based on confidence and torch hint
      if (autoBlend && (resultMean.isFinite() || chromVal.isFinite())) {
        var wTorch = if (torchOnHint) 0.0 else 1.0
        val wSnr = confChrom
        var w = (0.7 * wSnr + 0.3 * wTorch).coerceIn(0.0, 1.0)
        if (!chromVal.isFinite()) w = 0.0
        if (!resultMean.isFinite()) w = 1.0
        val mv = if (resultMean.isFinite()) resultMean else 0.0
        val cv = if (chromVal.isFinite()) chromVal else 0.0
        outVal = (1.0 - w) * mv + w * cv
        outConf = (1.0 - w) * confMean + w * confChrom
        blendWeight = w
        blendUsed = true
      }

      if (!windowMean.isFinite() && resultMean.isFinite()) windowMean = resultMean
      if (!dcMean.isFinite() && windowMean.isFinite()) dcMean = windowMean
      val prevDc = if (dcMean.isFinite()) dcMean else windowMean
      val alphaDc = if (histCount >= 16) 0.03 else 0.06
      val nextDc = if (windowMean.isFinite()) prevDc + alphaDc * (windowMean - prevDc) else prevDc
      dcMean = nextDc
      val targetStdCounts = 12.0
      val temporalBase = if (temporalStd.isFinite() && temporalStd > 0.0) temporalStd else 1.0
      val gainMean = (targetStdCounts / temporalBase).coerceIn(1.0, 6.0)
      val meanDenominator = kotlin.math.max(10.0, 60.0 / gainMean)
      val meanComponent = if (resultMean.isFinite() && nextDc.isFinite()) ((resultMean - nextDc) / meanDenominator).coerceIn(-1.2, 1.2) else Double.NaN

      val chromBase = if (chromAmp.isFinite() && chromAmp > 0.0) chromAmp else 1.0
      val chromGain = (18.0 / chromBase).coerceIn(1.0, 6.0)
      val chromDenominator = kotlin.math.max(15.0, 100.0 / chromGain)
      val chromComponent = if (chromVal.isFinite()) ((chromVal - 128.0) / chromDenominator).coerceIn(-1.2, 1.2) else Double.NaN
      val pushRaw = when {
        blendUsed -> {
          val mc = if (meanComponent.isFinite()) meanComponent else 0.0
          val cc = if (chromComponent.isFinite()) chromComponent else 0.0
          (1.0 - blendWeight) * mc + blendWeight * cc
        }
        mode == "chrom" || mode == "pos" -> chromComponent
        else -> meanComponent
      }
      val enableAgc = (params?.get("enableAgc") as? Boolean) ?: true
      val targetRms = (params?.get("targetRms") as? Number)?.toDouble() ?: DEFAULT_TARGET_RMS
      val alphaRms = (params?.get("alphaRms") as? Number)?.toDouble() ?: DEFAULT_ALPHA_RMS
      val alphaGain = (params?.get("alphaGain") as? Number)?.toDouble() ?: DEFAULT_ALPHA_GAIN
      val gainMin = (params?.get("gainMin") as? Number)?.toDouble() ?: DEFAULT_GAIN_MIN
      val gainMax = (params?.get("gainMax") as? Number)?.toDouble() ?: DEFAULT_GAIN_MAX
      val minRms = kotlin.math.max(targetRms * MIN_RMS_FRACTION, 0.001)

      var finalSample = pushRaw
      if (!finalSample.isFinite()) finalSample = Double.NaN

      var processedSample = finalSample
      var confidenceOut = 0.0
      if (processedSample.isFinite()) {
        signalDc = if (signalDc.isFinite()) signalDc + 0.02 * (processedSample - signalDc) else processedSample
        val highPassed = processedSample - signalDc

        var agcSample = highPassed
        if (enableAgc) {
          val prevRmsSq = if (agcRms.isFinite()) agcRms * agcRms else kotlin.math.abs(highPassed)
          val newRmsSq = (1.0 - alphaRms) * prevRmsSq + alphaRms * highPassed * highPassed
          agcRms = kotlin.math.sqrt(kotlin.math.max(newRmsSq, 0.0))
          val desiredGain = targetRms / kotlin.math.max(agcRms, minRms)
          val clampedGain = desiredGain.coerceIn(gainMin, gainMax)
          agcGain = if (agcGain.isFinite()) (1.0 - alphaGain) * agcGain + alphaGain * clampedGain else clampedGain
          agcSample = highPassed * agcGain
        }

        processedSample = agcSample.coerceIn(-0.6, 0.6)
        confidenceOut = kotlin.math.min(1.0, kotlin.math.abs(processedSample) / targetRms)
      }

      val pushSample = if (processedSample.isFinite()) processedSample else Double.NaN
      val finalConfidence = confidenceOut.coerceIn(0.0, 1.0)

      if (performanceLoggingFlag) {
        val elapsedMs = (System.nanoTime() - perfStartNanos) / 1_000_000.0
        recordFrameTiming(elapsedMs, simdActiveThisFrame)
      }

      if (histCount % 30 == 0) {
        Log.d(
          "PPGMeanPlugin",
          String.format(
            java.util.Locale.US,
            "AGC rms=%.4f gain=%.2f sample=%.4f",
            agcRms,
            agcGain,
            processedSample
          )
        )
      }

      // Publish sample + ts (seconds) to native buffer
      if (pushSample.isFinite()) {
        try {
          // Frame timestamp'i kullan, yoksa system time kullan
          val tsNanos = try { 
            frame.timestamp 
          } catch (_: Throwable) { 
            System.nanoTime() 
          }
          val tsSec = tsNanos.toDouble() / 1_000_000_000.0
          
          // Debug: Her 30 frame'de bir timestamp log'la
          if (histCount % 30 == 0) {
            Log.d("PPGPlugin", "PPG value: $pushSample, ts: $tsSec, conf: $finalConfidence")
          }
          
          HeartPyModule.addPPGSampleWithTs(pushSample, tsSec)
        } catch (_: Throwable) {}
      }
      try { HeartPyModule.addPPGSampleConfidence(finalConfidence) } catch (_: Throwable) {}
      pushSample
    } catch (t: Throwable) {
      java.lang.Double.NaN
    }
  }

  private data class SimdStats(val sum: Double, val sumSq: Double, val count: Int)

  private fun computeSampleCount(length: Int, step: Int): Int {
    if (length <= 0 || step <= 0) return 0
    return (length + step - 1) / step
  }

  private fun sumChannelSimd(
    buffer: ByteBuffer,
    baseOffset: Int,
    rowStride: Int,
    roiWidth: Int,
    roiHeight: Int,
    pixelStride: Int,
    channelOffset: Int,
    xStep: Int,
    yStep: Int,
  ): SimdStats? {
    if (!simdEnabledFlag || !nativeLibLoaded) return null
    if (!buffer.isDirect) return null
    if (roiWidth <= 0 || roiHeight <= 0) return null
    val cols = computeSampleCount(roiWidth, xStep)
    val rows = computeSampleCount(roiHeight, yStep)
    if (cols <= 0 || rows <= 0) return null
    return try {
      val result = nativeSumAndSquares(
        buffer,
        baseOffset,
        rowStride,
        roiWidth,
        roiHeight,
        pixelStride,
        channelOffset,
        xStep,
        yStep,
      )
      if (result == null || result.size < 2) {
        null
      } else {
        SimdStats(result[0], result[1], cols * rows)
      }
    } catch (t: Throwable) {
      Log.w("PPGMeanPlugin", "SIMD sum fallback: ${t.message}")
      null
    }
  }

  private fun updateParityStats(diff: Double) {
    if (diff > parityMaxDiff) {
      parityMaxDiff = diff
    }
    parityAccumDiff += diff
    paritySamples += 1
  }

  private fun resetParityStats() {
    parityMaxDiff = 0.0
    parityAccumDiff = 0.0
    paritySamples = 0
  }

  private fun percentile(sorted: DoubleArray, fraction: Double): Double {
    if (sorted.isEmpty()) return Double.NaN
    val clamped = fraction.coerceIn(0.0, 1.0)
    val index = (clamped * (sorted.size - 1)).toInt()
    return sorted[index]
  }

  private fun recordFrameTiming(elapsedMs: Double, simdActive: Boolean) {
    frameTimesMs[frameTimeIndex] = elapsedMs
    if (frameTimeCount < frameTimesMs.size) frameTimeCount++
    frameTimeIndex = (frameTimeIndex + 1) % frameTimesMs.size
    frameCounter += 1
    if (frameCounter % 100 == 0 && frameTimeCount > 0) {
      val sorted = frameTimesMs.copyOf(frameTimeCount)
      sorted.sort()
      val p50 = percentile(sorted, 0.5)
      val p95 = percentile(sorted, 0.95)
      val meanDiff = if (paritySamples > 0) parityAccumDiff / paritySamples.toDouble() else 0.0
      Log.d(
        "PPGMeanPlugin",
        String.format(
          Locale.US,
          "SIMD perf: frames=%d simd=%s p50=%.3fms p95=%.3fms diffMax=%.4f diffMean=%.4f samples=%d",
          frameTimeCount,
          if (simdActive) "ON" else "OFF",
          p50,
          p95,
          parityMaxDiff,
          meanDiff,
          paritySamples,
        ),
      )
      resetParityStats()
    }
  }

  companion object Registrar {
    private const val NATIVE_LIB_NAME = "ppg_simd"
    private const val PERF_WINDOW = 300
    private const val PARITY_INTERVAL = 10

    private var nativeLibLoaded = false

    init {
      nativeLibLoaded = try {
        System.loadLibrary(NATIVE_LIB_NAME)
        true
      } catch (error: UnsatisfiedLinkError) {
        Log.w("PPGMeanPlugin", "SIMD native library unavailable; using scalar fallback", error)
        false
      }
    }

    @JvmStatic
    private external fun nativeSumAndSquares(
      buffer: ByteBuffer,
      baseOffset: Int,
      bytesPerRow: Int,
      roiWidth: Int,
      roiHeight: Int,
      pixelStride: Int,
      channelOffset: Int,
      xStep: Int,
      yStep: Int,
    ): DoubleArray?

    @JvmStatic
    fun register() {
      FrameProcessorPluginRegistry.addFrameProcessorPlugin("ppgMean") { _, _ ->
        PPGMeanPlugin()
      }
    }
  }
}
