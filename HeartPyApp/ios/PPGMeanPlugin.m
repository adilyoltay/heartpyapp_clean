#import <Foundation/Foundation.h>
#import <VisionCamera/FrameProcessorPlugin.h>
#import <CoreVideo/CoreVideo.h>
#import <Accelerate/Accelerate.h>
#include <math.h>

@interface PPGMeanPlugin : FrameProcessorPlugin
@end

// SIMD buffers shared across helpers
static float* g_simdScratchBuffer = NULL;
static size_t g_simdScratchSize = 0;

// SIMD helper function for BGRA green channel processing
static double processBGRARoiWithSIMD(uint8_t* base, size_t bytesPerRow, 
                                    size_t px0, size_t py0, size_t px1, size_t py1,
                                    size_t xStep, size_t yStep, NSString* channel) {
  // Ensure scratch buffer is large enough
  size_t maxWidth = px1 - px0;
  if (g_simdScratchSize < maxWidth) {
    if (g_simdScratchBuffer) {
      free(g_simdScratchBuffer);
    }
    g_simdScratchSize = maxWidth;
    g_simdScratchBuffer = (float*)malloc(g_simdScratchSize * sizeof(float));
  }
  
  double totalSum = 0.0;
  size_t totalCount = 0;
  
  for (size_t y = py0; y < py1; y += yStep) {
    uint8_t* row = base + y * bytesPerRow;
    size_t rowWidth = px1 - px0;
    
    // Extract green channel with stride 4 using vDSP
    vDSP_vfltu8((const uint8_t*)row + px0 * 4 + 1, 4, g_simdScratchBuffer, 1, rowWidth);
    
    // Calculate sum for this row
    float rowSum = 0.0f;
    vDSP_sve(g_simdScratchBuffer, 1, &rowSum, rowWidth);
    
    totalSum += rowSum;
    totalCount += rowWidth;
  }
  
  return (totalCount > 0) ? (totalSum / totalCount) : 0.0;
}

// Scalar fallback function for comparison
static double processBGRARoiScalar(uint8_t* base, size_t bytesPerRow,
                                  size_t px0, size_t py0, size_t px1, size_t py1,
                                  size_t xStep, size_t yStep, NSString* channel) {
  unsigned long long sumR = 0, sumG = 0, sumB = 0;
  unsigned long long cnt = 0;
  
  for (size_t y = py0; y < py1; y += yStep) {
    uint8_t* row = base + y * bytesPerRow;
    for (size_t x = px0; x < px1; x += xStep) {
      uint8_t b = row[x * 4 + 0];
      uint8_t g = row[x * 4 + 1];
      uint8_t r = row[x * 4 + 2];
      sumR += r; sumG += g; sumB += b; cnt++;
    }
  }
  
  if (cnt == 0) return 0.0;
  
  double Rm = (double)sumR / (double)cnt;
  double Gm = (double)sumG / (double)cnt;
  double Bm = (double)sumB / (double)cnt;
  
  if ([channel isEqualToString:@"red"]) return Rm;
  if ([channel isEqualToString:@"luma"]) return 0.114 * Bm + 0.587 * Gm + 0.299 * Rm;
  return Gm; // default to green
}

@implementation PPGMeanPlugin

// SIMD optimization buffers and performance tracking
static BOOL g_simdEnabled = YES; // Default enabled
static BOOL g_performanceLogging = NO; // Default disabled

// Performance measurement arrays
static double g_frameTimes[300]; // Store 300 frame times for p50/p95 calculation
static int g_frameTimeIndex = 0;
static int g_frameTimeCount = 0;

- (id)callback:(Frame *)frame withArguments:(NSDictionary *)arguments {
  // Performance measurement start
  CFTimeInterval startTime = CACurrentMediaTime();
  
  // SIMD flag control
  NSNumber* simdFlag = arguments[@"simdEnabled"];
  if (simdFlag != nil) {
    g_simdEnabled = [simdFlag boolValue];
  }
  
  NSNumber* perfLogFlag = arguments[@"performanceLogging"];
  if (perfLogFlag != nil) {
    g_performanceLogging = [perfLogFlag boolValue];
  }
  
  // ROI fraction (0..1)
  NSNumber* roiNum = arguments[@"roi"];
  double roiIn = roiNum != nil ? roiNum.doubleValue : 0.4;
  double roi = fmax(0.2, fmin(0.6, roiIn));

  // Channel: "green" | "red" | "luma" (default: green for rPPG SNR)
  NSString* channel = [arguments objectForKey:@"channel"];
  if (![channel isKindOfClass:[NSString class]] || channel.length == 0) {
    channel = @"green";
  }

  // Mode: "mean" | "chrom" | "pos" (pos aliases chrom for v1)
  NSString* mode = [arguments objectForKey:@"mode"];
  if (![mode isKindOfClass:[NSString class]] || mode.length == 0) mode = @"mean";
  BOOL useCHROM = ([mode isEqualToString:@"chrom"] || [mode isEqualToString:@"pos"]);
  // Blend: "off" | "auto" (auto crossfades mean vs chrom/pos)
  NSString* blend = [arguments objectForKey:@"blend"];
  BOOL autoBlend = [blend isKindOfClass:[NSString class]] && [blend isEqualToString:@"auto"];
  // Torch hint from JS for scenario weighting
  NSNumber* torchNum = arguments[@"torch"];
  BOOL torchOnHint = torchNum != nil ? [torchNum boolValue] : NO;

  // Grid size for multi-ROI (1..3)
  NSNumber* gridNum = arguments[@"grid"];
  int grid = gridNum != nil ? [gridNum intValue] : 1;
  if (grid < 1) grid = 1; if (grid > 3) grid = 3;

  // Sampling step (stride) for performance
  NSNumber* stepNum = arguments[@"step"];
  int stepIn = stepNum != nil ? stepNum.intValue : 2;
  int step = stepIn < 1 ? 1 : (stepIn > 8 ? 8 : stepIn);

  CMSampleBufferRef buffer = frame.buffer;
  CVImageBufferRef pixelBuffer = CMSampleBufferGetImageBuffer(buffer);
  if (pixelBuffer == nil) return @(NAN);

  CVPixelBufferLockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
  OSType type = CVPixelBufferGetPixelFormatType(pixelBuffer);
  size_t width = CVPixelBufferGetWidth(pixelBuffer);
  size_t height = CVPixelBufferGetHeight(pixelBuffer);

  // Compute centered ROI
  size_t roiW = MAX((size_t)1, (size_t)(width * roi));
  size_t roiH = MAX((size_t)1, (size_t)(height * roi));
  // Area guard: ensure ROI covers at least 10% of frame
  double minArea = 0.1 * (double)width * (double)height;
  if ((double)roiW * (double)roiH < minArea) {
    roi = 0.4;
    roiW = MAX((size_t)1, (size_t)(width * roi));
    roiH = MAX((size_t)1, (size_t)(height * roi));
  }
  size_t startX = (width > roiW) ? (width - roiW) / 2 : 0;
  size_t startY = (height > roiH) ? (height - roiH) / 2 : 0;

  double outSample = NAN;
  double outConfidence = NAN;
  // Rolling history for CHROM/POS (aggregated ROI means)
  static const int HP_HIST_N = 64;
static double rHist[HP_HIST_N];
static double gHist[HP_HIST_N];
static double bHist[HP_HIST_N];
static double yHist[HP_HIST_N];
static double vHist[HP_HIST_N];
static int histPos = 0;
static int histCount = 0;
static double dcMean = NAN;

  // Multi-ROI aggregation with simple exposure-based weighting
  double weightedSum = 0.0;
  double weightTotal = 0.0;
  double confAccum = 0.0;
  double wSumR = 0.0, wSumG = 0.0, wSumB = 0.0, wSumY = 0.0;
  double spatialSum = 0.0, spatialSqSum = 0.0, spatialSamples = 0.0;

  if (type == kCVPixelFormatType_32BGRA) {
    uint8_t* base = (uint8_t*)CVPixelBufferGetBaseAddress(pixelBuffer);
    const size_t bytesPerRow = CVPixelBufferGetBytesPerRow(pixelBuffer);
    const size_t xStep = (size_t)step, yStep = (size_t)step;
    size_t patchW = MAX((size_t)1, roiW / (size_t)grid);
    size_t patchH = MAX((size_t)1, roiH / (size_t)grid);
    
    for (int gy = 0; gy < grid; ++gy) {
      for (int gx = 0; gx < grid; ++gx) {
        size_t px0 = startX + (size_t)gx * patchW;
        size_t py0 = startY + (size_t)gy * patchH;
        size_t px1 = (gx == grid - 1) ? (startX + roiW) : (px0 + patchW);
        size_t py1 = (gy == grid - 1) ? (startY + roiH) : (py0 + patchH);
        
        double value = 0.0;
        double Rm = 0.0, Gm = 0.0, Bm = 0.0;
        unsigned long long cnt = 0;
        
        if (g_simdEnabled) {
          // Use SIMD optimization for green channel
          Gm = processBGRARoiWithSIMD(base, bytesPerRow, px0, py0, px1, py1, xStep, yStep, channel);
          
          // For red and luma, we still need scalar processing
          if ([channel isEqualToString:@"red"] || [channel isEqualToString:@"luma"]) {
            unsigned long long sumR = 0, sumG = 0, sumB = 0;
            cnt = 0;
            for (size_t y = py0; y < py1; y += yStep) {
              uint8_t* row = base + y * bytesPerRow;
              for (size_t x = px0; x < px1; x += xStep) {
                uint8_t b = row[x * 4 + 0];
                uint8_t g = row[x * 4 + 1];
                uint8_t r = row[x * 4 + 2];
                sumR += r; sumG += g; sumB += b; cnt++;
              }
            }
            if (cnt > 0) {
              Rm = (double)sumR / (double)cnt;
              Gm = (double)sumG / (double)cnt;
              Bm = (double)sumB / (double)cnt;
            }
          } else {
            // For green channel, calculate count for SIMD result
            cnt = ((px1 - px0) / xStep) * ((py1 - py0) / yStep);
          }
        } else {
          // Scalar fallback
          unsigned long long sumR = 0, sumG = 0, sumB = 0;
          cnt = 0;
          for (size_t y = py0; y < py1; y += yStep) {
            uint8_t* row = base + y * bytesPerRow;
            for (size_t x = px0; x < px1; x += xStep) {
              uint8_t b = row[x * 4 + 0];
              uint8_t g = row[x * 4 + 1];
              uint8_t r = row[x * 4 + 2];
              sumR += r; sumG += g; sumB += b; cnt++;
            }
          }
          if (cnt > 0) {
            Rm = (double)sumR / (double)cnt;
            Gm = (double)sumG / (double)cnt;
            Bm = (double)sumB / (double)cnt;
          }
        }
        
        if (cnt == 0) continue;
        
        double Ym = 0.114 * Bm + 0.587 * Gm + 0.299 * Rm;
        value = ([channel isEqualToString:@"red"]) ? Rm : ([channel isEqualToString:@"luma"]) ? Ym : Gm;
        if (value < 0.0) value = 0.0; if (value > 255.0) value = 255.0;
        // exposure score from luma
        double expScore = 1.0;
        if (Ym < 15.0) expScore = fmax(0.0, Ym / 15.0);
        else if (Ym > 240.0) expScore = fmax(0.0, (255.0 - Ym) / 15.0);
        double ampScore = 0.0; // no temporal history; keep small proxy
        if (useCHROM) {
          double Sabs = fabs((3.0 * Rm - 2.0 * Gm) - (1.5 * Rm + 1.0 * Gm - 1.5 * Bm));
          ampScore = fmin(1.0, Sabs / 50.0);
        }
        double conf = fmin(1.0, fmax(0.0, 0.7 * expScore + 0.3 * ampScore));
        double w = fmax(1e-6, expScore);
        weightedSum += w * value; weightTotal += w; confAccum += w * conf;
        wSumR += w * Rm; wSumG += w * Gm; wSumB += w * Bm; wSumY += w * Ym;
        spatialSum += Ym;
        spatialSqSum += Ym * Ym;
        spatialSamples += 1.0;
      }
    }
  } else {
    // NV12 Y + interleaved CbCr
    const size_t yPlane = 0;
    const size_t uvPlane = 1;
    const size_t yRowStride = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, yPlane);
    const size_t uvRowStride = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, uvPlane);
    uint8_t* yBase = (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, yPlane);
    uint8_t* uvBase = (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, uvPlane);
    const size_t xStep = (size_t)step, yStep = (size_t)step;
    size_t patchW = MAX((size_t)1, roiW / (size_t)grid);
    size_t patchH = MAX((size_t)1, roiH / (size_t)grid);
    for (int gy = 0; gy < grid; ++gy) {
      for (int gx = 0; gx < grid; ++gx) {
        size_t px0 = startX + (size_t)gx * patchW;
        size_t py0 = startY + (size_t)gy * patchH;
        size_t px1 = (gx == grid - 1) ? (startX + roiW) : (px0 + patchW);
        size_t py1 = (gy == grid - 1) ? (startY + roiH) : (py0 + patchH);
        unsigned long long sumR = 0, sumG = 0, sumB = 0, sumY = 0; unsigned long long cnt = 0;
        for (size_t y = py0; y < py1; y += yStep) {
          uint8_t* yRow = yBase + y * yRowStride;
          size_t uvY = y >> 1; uint8_t* uvRow = uvBase + uvY * uvRowStride;
          for (size_t x = px0; x < px1; x += xStep) {
            size_t yIdx = x; double Yv = (double)(yRow[yIdx] & 0xFF);
            size_t uvX = x >> 1; size_t uvIdx = uvX * 2; uint8_t Cb = uvRow[uvIdx + 0]; uint8_t Cr = uvRow[uvIdx + 1];
            double cb = (double)Cb - 128.0; double cr = (double)Cr - 128.0;
            double Rv = Yv + 1.402 * cr; double Gv = Yv - 0.344 * cb - 0.714 * cr; double Bv = Yv + 1.772 * cb;
            if (Rv < 0.0) Rv = 0.0; if (Rv > 255.0) Rv = 255.0;
            if (Gv < 0.0) Gv = 0.0; if (Gv > 255.0) Gv = 255.0;
            if (Bv < 0.0) Bv = 0.0; if (Bv > 255.0) Bv = 255.0;
            sumR += (unsigned long long)Rv; sumG += (unsigned long long)Gv; sumB += (unsigned long long)Bv; sumY += (unsigned long long)Yv; cnt++;
          }
        }
        if (cnt == 0) continue;
        double Rm = (double)sumR / (double)cnt; double Gm = (double)sumG / (double)cnt; double Bm = (double)sumB / (double)cnt; double Ym = (double)sumY / (double)cnt;
        double value = ([channel isEqualToString:@"red"]) ? Rm : ([channel isEqualToString:@"luma"]) ? Ym : Gm;
        if (value < 0.0) value = 0.0; if (value > 255.0) value = 255.0;
        double expScore = 1.0; if (Ym < 15.0) expScore = fmax(0.0, Ym / 15.0); else if (Ym > 240.0) expScore = fmax(0.0, (255.0 - Ym) / 15.0);
        double ampScore = 0.0; if (useCHROM) { double Sabs = fabs((3.0 * Rm - 2.0 * Gm) - (1.5 * Rm + 1.0 * Gm - 1.5 * Bm)); ampScore = fmin(1.0, Sabs / 50.0);} 
        double conf = fmin(1.0, fmax(0.0, 0.7 * expScore + 0.3 * ampScore));
        double w = fmax(1e-6, expScore);
        weightedSum += w * value; weightTotal += w; confAccum += w * conf;
        wSumR += w * Rm; wSumG += w * Gm; wSumB += w * Bm; wSumY += w * Ym;
        spatialSum += Ym;
        spatialSqSum += Ym * Ym;
        spatialSamples += 1.0;
      }
    }
  }

  double mean = (weightTotal > 0.0) ? (weightedSum / weightTotal) : NAN;
  double Ragg = (weightTotal > 0.0) ? (wSumR / weightTotal) : NAN;
  double Gagg = (weightTotal > 0.0) ? (wSumG / weightTotal) : NAN;
  double Bagg = (weightTotal > 0.0) ? (wSumB / weightTotal) : NAN;
  double Yagg = (weightTotal > 0.0) ? (wSumY / weightTotal) : NAN;

  double spatialMean = (spatialSamples > 0.0) ? (spatialSum / spatialSamples) : NAN;
  double spatialVar = (spatialSamples > 0.0) ? (spatialSqSum / spatialSamples) - spatialMean * spatialMean : 0.0;
  if (!isfinite(spatialVar) || spatialVar < 0.0) spatialVar = 0.0;
  double spatialStd = sqrt(spatialVar);
  double contrastScore = fmin(1.0, fmax(0.0, spatialStd / 12.0));

  // Update rolling history
  if (isfinite(Ragg) && isfinite(Gagg) && isfinite(Bagg) && isfinite(Yagg)) {
    rHist[histPos] = Ragg; gHist[histPos] = Gagg; bHist[histPos] = Bagg; yHist[histPos] = Yagg; vHist[histPos] = mean;
    histPos = (histPos + 1) % HP_HIST_N;
    if (histCount < HP_HIST_N) histCount++;
  }

  // Compute outSample based on mode using rolling alpha (CHROM) or POS
  double chromVal = NAN;
  double chromAmp = 0.0;
  if (histCount >= 8) {
    // Build arrays over available window
    int N = histCount;
    // Compute X, Y over window
    double stdX = 0.0, stdYc = 0.0, meanX = 0.0, meanYc = 0.0;
    for (int i = 0; i < N; ++i) {
      int idx = (histPos - 1 - i + HP_HIST_N) % HP_HIST_N;
      double Rv = rHist[idx], Gv = gHist[idx], Bv = bHist[idx];
      double Xv = 3.0 * Rv - 2.0 * Gv;
      double Ycv = 1.5 * Rv + 1.0 * Gv - 1.5 * Bv;
      meanX += Xv; meanYc += Ycv;
    }
    meanX /= N; meanYc /= N;
    for (int i = 0; i < N; ++i) {
      int idx = (histPos - 1 - i + HP_HIST_N) % HP_HIST_N;
      double Rv = rHist[idx], Gv = gHist[idx], Bv = bHist[idx];
      double Xv = 3.0 * Rv - 2.0 * Gv;
      double Ycv = 1.5 * Rv + 1.0 * Gv - 1.5 * Bv;
      stdX += (Xv - meanX) * (Xv - meanX);
      stdYc += (Ycv - meanYc) * (Ycv - meanYc);
    }
    stdX = sqrt(stdX / fmax(1, N - 1));
    stdYc = sqrt(stdYc / fmax(1, N - 1));
    chromAmp = stdX;
    double alpha = (stdYc > 1e-6) ? (stdX / stdYc) : 1.0;

    // Current X,Y values
    int lastIdx = (histPos - 1 + HP_HIST_N) % HP_HIST_N;
    double Rv = rHist[lastIdx], Gv = gHist[lastIdx], Bv = bHist[lastIdx];
    double Xcur = 3.0 * Rv - 2.0 * Gv;
    double Ycur = 1.5 * Rv + 1.0 * Gv - 1.5 * Bv;

    if ([mode isEqualToString:@"chrom"] || [mode isEqualToString:@"pos"]) {
      double Sc = 0.0;
      if ([mode isEqualToString:@"chrom"]) {
        Sc = Xcur - alpha * Ycur;
      } else {
        // POS: normalize RGB over window, compute S1,S2; S = S1_last + alpha_pos * S2_last
        double meanR = 0.0, meanG = 0.0, meanB = 0.0;
        for (int i = 0; i < N; ++i) {
          int idx = (histPos - 1 - i + HP_HIST_N) % HP_HIST_N;
          meanR += rHist[idx]; meanG += gHist[idx]; meanB += bHist[idx];
        }
        meanR /= N; meanG /= N; meanB /= N;
        double s1Mean = 0.0, s2Mean = 0.0;
        for (int i = 0; i < N; ++i) {
          int idx = (histPos - 1 - i + HP_HIST_N) % HP_HIST_N;
          double rN = (rHist[idx] / fmax(1e-6, meanR)) - 1.0;
          double gN = (gHist[idx] / fmax(1e-6, meanG)) - 1.0;
          double bN = (bHist[idx] / fmax(1e-6, meanB)) - 1.0;
          double s1 = 3.0 * rN - 2.0 * gN;
          double s2 = 1.5 * rN + 1.0 * gN - 1.5 * bN;
          s1Mean += s1; s2Mean += s2;
        }
        s1Mean /= N; s2Mean /= N;
        double var1 = 0.0, var2 = 0.0;
        for (int i = 0; i < N; ++i) {
          int idx = (histPos - 1 - i + HP_HIST_N) % HP_HIST_N;
          double rN = (rHist[idx] / fmax(1e-6, meanR)) - 1.0;
          double gN = (gHist[idx] / fmax(1e-6, meanG)) - 1.0;
          double bN = (bHist[idx] / fmax(1e-6, meanB)) - 1.0;
          double s1 = 3.0 * rN - 2.0 * gN;
          double s2 = 1.5 * rN + 1.0 * gN - 1.5 * bN;
          var1 += (s1 - s1Mean) * (s1 - s1Mean);
          var2 += (s2 - s2Mean) * (s2 - s2Mean);
        }
        double std1 = sqrt(var1 / fmax(1, N - 1));
        double std2 = sqrt(var2 / fmax(1, N - 1));
        double alphaPos = (std2 > 1e-6) ? (std1 / std2) : 1.0;
        // current normalized values
        double rN = (Rv / fmax(1e-6, meanR)) - 1.0;
        double gN = (Gv / fmax(1e-6, meanG)) - 1.0;
        double bN = (Bv / fmax(1e-6, meanB)) - 1.0;
        double s1Last = 3.0 * rN - 2.0 * gN;
        double s2Last = 1.5 * rN + 1.0 * gN - 1.5 * bN;
        Sc = s1Last + alphaPos * s2Last;
      }
      // map to 0..255 around 128
      double k = 0.5; // conservative scale
      chromVal = 128.0 + k * Sc;
      if (chromVal < 0.0) chromVal = 0.0; if (chromVal > 255.0) chromVal = 255.0;
    }
  }
  // Default selection by mode
  outSample = isfinite(mean) ? mean : chromVal;
  if (![mode isEqualToString:@"mean"] && isfinite(chromVal)) outSample = chromVal;
  double blendWeight = 0.0; bool blendUsed = false;

  double windowMean = isfinite(Yagg) ? Yagg : mean;
  double temporalScore = 0.2;
  double temporalStd = 0.0;
  if (histCount >= 6) {
    int window = MIN(histCount, 30);
    double meanHist = 0.0;
    for (int i = 0; i < window; ++i) {
      int idx = (histPos - 1 - i + HP_HIST_N) % HP_HIST_N;
      meanHist += vHist[idx];
    }
    meanHist /= (double)window;
    windowMean = meanHist;
    double varHist = 0.0;
    for (int i = 0; i < window; ++i) {
      int idx = (histPos - 1 - i + HP_HIST_N) % HP_HIST_N;
      double d = vHist[idx] - meanHist;
      varHist += d * d;
    }
    temporalStd = sqrt(varHist / fmax(1, window - 1));
    temporalScore = fmin(1.0, fmax(0.0, temporalStd / 6.0));
  }
  if (!isfinite(temporalScore) || temporalScore < 0.0) temporalScore = 0.0;
  double amplitudeScore = fmin(1.0, fmax(0.0, chromAmp / 35.0));

  // Dynamic exposure gating via Y percentiles
  double expoGate = 1.0;
  if (histCount >= 16) {
    int N = histCount;
    double tmp[HP_HIST_N];
    for (int i = 0; i < N; ++i) {
      int idx = (histPos - 1 - i + HP_HIST_N) % HP_HIST_N;
      tmp[i] = yHist[idx];
    }
    // simple selection sort for small N to get p10/p90
    for (int i = 0; i < N; ++i) {
      int m = i;
      for (int j = i + 1; j < N; ++j) if (tmp[j] < tmp[m]) m = j;
      double t = tmp[i]; tmp[i] = tmp[m]; tmp[m] = t;
    }
    int i10 = (int)floor(0.1 * (N - 1));
    int i90 = (int)floor(0.9 * (N - 1));
    double p10 = tmp[i10]; double p90 = tmp[i90];
    double gDark = fmin(1.0, fmax(0.0, p10 / 20.0));
    double gSat = fmin(1.0, fmax(0.0, (255.0 - p90) / 20.0));
    expoGate = fmin(gDark, gSat);
  }

  double expoScore = fmin(1.0, fmax(0.0, expoGate));
  double patchScore = (weightTotal > 0.0) ? fmin(1.0, fmax(0.0, confAccum / weightTotal)) : expoScore;
  double spatialGate = fmin(1.0, fmax(0.0, 0.6 * expoScore + 0.4 * contrastScore));
  double dynamicMix = fmin(1.0, fmax(0.0, 0.7 * temporalScore + 0.3 * amplitudeScore));
  double reliability = sqrt(fmin(1.0, fmax(0.0, spatialGate * dynamicMix)));
  double baseConf = fmin(1.0, fmax(0.0, 0.3 * patchScore + 0.7 * reliability));
  double confMean = baseConf;
  double confChrom = fmin(1.0, fmax(0.0, 0.6 * baseConf + 0.4 * dynamicMix));
  // Auto blend: crossfade mean vs chrom/pos using confidence and torch hint
  if (autoBlend && (isfinite(chromVal) || isfinite(mean))) {
    double wTorch = torchOnHint ? 0.0 : 1.0;
    double wSnr = confChrom;
    double w = fmin(1.0, fmax(0.0, 0.7 * wSnr + 0.3 * wTorch));
    if (!isfinite(chromVal)) w = 0.0;
    if (!isfinite(mean)) w = 1.0;
    outSample = (1.0 - w) * (isfinite(mean) ? mean : 0.0) + w * (isfinite(chromVal) ? chromVal : 0.0);
    outConfidence = (1.0 - w) * confMean + w * confChrom;
    blendWeight = w; blendUsed = true;
  } else {
    outConfidence = fmin(1.0, fmax(0.0, 0.5 * baseConf + 0.5 * expoGate));
  }

  if (!isfinite(windowMean) && isfinite(mean)) windowMean = mean;
  if (!isfinite(dcMean) && isfinite(windowMean)) dcMean = windowMean;
  double prevDc = isfinite(dcMean) ? dcMean : windowMean;
  
  // IMPROVED DC TRACKING: Faster adaptation during saturation/large changes
  double dcError = fabs(windowMean - prevDc);
  double alphaDc = (histCount >= 16) ? 0.03 : 0.06;
  
  // Accelerate DC adaptation for large errors (saturation recovery)
  if (dcError > 20.0) {
    alphaDc = fmin(0.15, alphaDc * 3.0); // 3x faster for large errors
  } else if (dcError > 10.0) {
    alphaDc = fmin(0.10, alphaDc * 2.0); // 2x faster for medium errors
  }
  
  double nextDc = isfinite(windowMean) ? prevDc + alphaDc * (windowMean - prevDc) : prevDc;
  dcMean = nextDc;
  const double targetStdCounts = 12.0;
  double meanDenominator = 60.0;
  if (isfinite(temporalStd) && temporalStd >= 0.0) {
    double gain = targetStdCounts / fmax(temporalStd, 1.0);
    gain = fmin(6.0, fmax(1.0, gain));
    meanDenominator = fmax(10.0, 60.0 / gain);
  } else {
    meanDenominator = 10.0;
  }

  double chromDenominator = 100.0;
  if (isfinite(chromAmp) && chromAmp > 0.0) {
    double chromGain = 18.0 / fmax(chromAmp, 1.0);
    chromGain = fmin(6.0, fmax(1.0, chromGain));
    chromDenominator = fmax(15.0, 100.0 / chromGain);
  } else {
    chromDenominator = 15.0;
  }

  double meanComponent = (isfinite(mean) && isfinite(nextDc)) ? fmax(-1.2, fmin(1.2, (mean - nextDc) / meanDenominator)) : NAN;
  double chromComponent = isfinite(chromVal) ? fmax(-1.2, fmin(1.2, (chromVal - 128.0) / chromDenominator)) : NAN;
  double finalSample = NAN;
  if (blendUsed) {
    double mc = isfinite(meanComponent) ? meanComponent : 0.0;
    double cc = isfinite(chromComponent) ? chromComponent : 0.0;
    finalSample = (1.0 - blendWeight) * mc + blendWeight * cc;
  } else if ([mode isEqualToString:@"chrom"] || [mode isEqualToString:@"pos"]) {
    finalSample = chromComponent;
  } else {
    finalSample = meanComponent;
  }

  BOOL enableAgc = arguments[@"enableAgc"] ? [arguments[@"enableAgc"] boolValue] : YES;
  double targetRms = arguments[@"targetRms"] ? [arguments[@"targetRms"] doubleValue] : 0.02;
  double alphaRms = arguments[@"alphaRms"] ? [arguments[@"alphaRms"] doubleValue] : 0.05;
  double alphaGain = arguments[@"alphaGain"] ? [arguments[@"alphaGain"] doubleValue] : 0.1;
  double gainMin = arguments[@"gainMin"] ? [arguments[@"gainMin"] doubleValue] : 0.5;
  double gainMax = arguments[@"gainMax"] ? [arguments[@"gainMax"] doubleValue] : 20.0;
  double minRms = fmax(targetRms * 0.1, 0.001);

  static double agcRms = 0.0;
  static double agcGain = 1.0;
  static double signalDc = NAN;

  double processedSample = finalSample;
  double finalConfidence = 0.0;
  if (isfinite(processedSample)) {
    if (!isfinite(signalDc)) {
      signalDc = processedSample;
    } else {
      signalDc = signalDc + 0.02 * (processedSample - signalDc);
    }
    double highPassed = processedSample - signalDc;
    double agcSample = highPassed;
    if (enableAgc) {
      double prevRmsSq = isfinite(agcRms) ? agcRms * agcRms : fabs(highPassed);
      double newRmsSq = (1.0 - alphaRms) * prevRmsSq + alphaRms * highPassed * highPassed;
      agcRms = sqrt(fmax(newRmsSq, 0.0));
      double desiredGain = targetRms / fmax(agcRms, minRms);
      double clampedGain = fmin(gainMax, fmax(gainMin, desiredGain));
      agcGain = isfinite(agcGain) ? (1.0 - alphaGain) * agcGain + alphaGain * clampedGain : clampedGain;
      agcSample = highPassed * agcGain;
    }
    processedSample = fmax(-0.6, fmin(0.6, agcSample));
    finalConfidence = fmin(1.0, fabs(processedSample) / targetRms);
  }

  double pushSample = isfinite(processedSample) ? processedSample : NAN;

  // Performance measurement and logging
  CFTimeInterval endTime = CACurrentMediaTime();
  double frameTimeMs = (endTime - startTime) * 1000.0;
  
  // Store frame time for p50/p95 calculation
  g_frameTimes[g_frameTimeIndex] = frameTimeMs;
  g_frameTimeIndex = (g_frameTimeIndex + 1) % 300;
  if (g_frameTimeCount < 300) g_frameTimeCount++;
  
  // Log performance metrics every 100 frames when enabled
  if (g_performanceLogging && g_frameTimeCount > 0 && g_frameTimeCount % 100 == 0) {
    // Calculate p50 and p95
    double sortedTimes[300];
    memcpy(sortedTimes, g_frameTimes, g_frameTimeCount * sizeof(double));
    
    // Simple bubble sort for small arrays
    for (int i = 0; i < g_frameTimeCount - 1; i++) {
      for (int j = 0; j < g_frameTimeCount - i - 1; j++) {
        if (sortedTimes[j] > sortedTimes[j + 1]) {
          double temp = sortedTimes[j];
          sortedTimes[j] = sortedTimes[j + 1];
          sortedTimes[j + 1] = temp;
        }
      }
    }
    
    int p50Index = (int)(g_frameTimeCount * 0.5);
    int p95Index = (int)(g_frameTimeCount * 0.95);
    double p50 = sortedTimes[p50Index];
    double p95 = sortedTimes[p95Index];
    
    NSLog(@"📊 PPGMeanPlugin Performance: SIMD=%@, Frames=%d, P50=%.2fms, P95=%.2fms", 
          g_simdEnabled ? @"ON" : @"OFF", g_frameTimeCount, p50, p95);
  }

  CVPixelBufferUnlockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);

  // Post native notification so HeartPyModule can collect real samples without JS bridge
  @try {
    static int notificationCount = 0;
    notificationCount++;
    NSLog(@"📸 PPGMeanPlugin: Processing frame #%d", notificationCount);
    NSLog(@"📸 PPGMeanPlugin: pushSample=%.4f, isfinite=%d, finalConfidence=%.2f",
          pushSample, isfinite(pushSample), finalConfidence);

    if (isfinite(pushSample)) {
      NSDictionary* userInfo = @{ @"value": @(pushSample),
                                  @"timestamp": @([[NSDate date] timeIntervalSince1970]),
                                  @"confidence": @(finalConfidence) };
      NSLog(@"📸 PPGMeanPlugin: About to post notification #%d value: %.3f confidence: %.2f",
            notificationCount, pushSample, finalConfidence);
      [[NSNotificationCenter defaultCenter] postNotificationName:@"HeartPyPPGSample"
                                                          object:nil
                                                        userInfo:userInfo];
      NSLog(@"📸 PPGMeanPlugin: Successfully posted notification #%d", notificationCount);

      if (notificationCount % 10 == 0) {  // More frequent logging
        NSLog(@"📸 PPGMeanPlugin posted notification #%d value: %.3f confidence: %.2f",
              notificationCount, pushSample, finalConfidence);
      }
      if (notificationCount % 10 == 0) {
        NSLog(@"📸 PPGMeanPlugin gain=%.2f rms=%.4f sample=%.4f",
              agcGain,
              agcRms,
              pushSample);
      }
    } else {
      NSLog(@"📸 PPGMeanPlugin: Skipping notification - sample not finite: %f", pushSample);
    }
  } @catch (__unused id e) {}
  return @(pushSample);
}

VISION_EXPORT_FRAME_PROCESSOR(PPGMeanPlugin, ppgMean)

@end
