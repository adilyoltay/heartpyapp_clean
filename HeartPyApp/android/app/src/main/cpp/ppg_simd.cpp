#include <jni.h>
#include <android/log.h>
#include <cstdint>
#include <algorithm>

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#define HAVE_NEON 1
#else
#define HAVE_NEON 0
#endif

namespace {
constexpr const char* kTag = "PPGSimdNative";

struct SumResult {
  uint64_t sum;
  uint64_t sumSq;
};

inline int computeSampleCount(int length, int step) {
  if (length <= 0 || step <= 0) {
    return 0;
  }
  return (length + step - 1) / step;
}

inline SumResult sumRowScalar(
    const uint8_t* rowPtr,
    int samples,
    int pixelStride,
    int channelOffset,
    int xStep) {
  SumResult result{0, 0};
  if (samples <= 0) {
    return result;
  }
  const uint8_t* ptr = rowPtr + channelOffset;
  int offset = 0;
  const int stride = pixelStride * xStep;
  for (int i = 0; i < samples; ++i, offset += stride) {
    const uint8_t value = ptr[offset];
    result.sum += static_cast<uint64_t>(value);
    result.sumSq += static_cast<uint64_t>(value) * static_cast<uint64_t>(value);
  }
  return result;
}

#if HAVE_NEON
inline uint64_t horizontalAddU32(uint32x4_t v) {
  const uint64x2_t pair = vpaddlq_u32(v);
  return static_cast<uint64_t>(vgetq_lane_u64(pair, 0)) +
         static_cast<uint64_t>(vgetq_lane_u64(pair, 1));
}

inline uint64_t horizontalAddU64(uint64x2_t v) {
  return static_cast<uint64_t>(vgetq_lane_u64(v, 0)) +
         static_cast<uint64_t>(vgetq_lane_u64(v, 1));
}

inline SumResult sumRowStride1Step1(const uint8_t* ptr, int samples) {
  SumResult result{0, 0};
  if (samples <= 0) {
    return result;
  }
  const int limit = samples & ~15;
  uint32x4_t sum32 = vdupq_n_u32(0);
  uint64x2_t sumSq64 = vdupq_n_u64(0);

  int i = 0;
  for (; i < limit; i += 16) {
    const uint8x16_t vec = vld1q_u8(ptr + i);
    const uint16x8_t lo = vmovl_u8(vget_low_u8(vec));
    const uint16x8_t hi = vmovl_u8(vget_high_u8(vec));

    sum32 = vaddq_u32(sum32, vpaddlq_u16(lo));
    sum32 = vaddq_u32(sum32, vpaddlq_u16(hi));

    const uint32x4_t loSq0 = vmull_u16(vget_low_u16(lo), vget_low_u16(lo));
    const uint32x4_t loSq1 = vmull_u16(vget_high_u16(lo), vget_high_u16(lo));
    const uint32x4_t hiSq0 = vmull_u16(vget_low_u16(hi), vget_low_u16(hi));
    const uint32x4_t hiSq1 = vmull_u16(vget_high_u16(hi), vget_high_u16(hi));

    uint32x4_t sq32 = vaddq_u32(loSq0, loSq1);
    sq32 = vaddq_u32(sq32, hiSq0);
    sq32 = vaddq_u32(sq32, hiSq1);
    sumSq64 = vaddq_u64(sumSq64, vpaddlq_u32(sq32));
  }

  result.sum += horizontalAddU32(sum32);
  result.sumSq += horizontalAddU64(sumSq64);

  for (; i < samples; ++i) {
    const uint8_t value = ptr[i];
    result.sum += static_cast<uint64_t>(value);
    result.sumSq += static_cast<uint64_t>(value) * static_cast<uint64_t>(value);
  }
  return result;
}

inline SumResult sumRowStride1Step2(const uint8_t* ptr, int samples, int xStep) {
  SumResult result{0, 0};
  if (samples <= 0) {
    return result;
  }
  const int limit = samples & ~15;
  uint32x4_t sum32 = vdupq_n_u32(0);
  uint64x2_t sumSq64 = vdupq_n_u64(0);

  int offsetBytes = 0;
  int i = 0;
  for (; i < limit; i += 16, offsetBytes += 32) {
    const uint8x16x2_t vec = vld2q_u8(ptr + offsetBytes);
    const uint8x16_t channel = vec.val[0];
    const uint16x8_t lo = vmovl_u8(vget_low_u8(channel));
    const uint16x8_t hi = vmovl_u8(vget_high_u8(channel));

    sum32 = vaddq_u32(sum32, vpaddlq_u16(lo));
    sum32 = vaddq_u32(sum32, vpaddlq_u16(hi));

    const uint32x4_t loSq0 = vmull_u16(vget_low_u16(lo), vget_low_u16(lo));
    const uint32x4_t loSq1 = vmull_u16(vget_high_u16(lo), vget_high_u16(lo));
    const uint32x4_t hiSq0 = vmull_u16(vget_low_u16(hi), vget_low_u16(hi));
    const uint32x4_t hiSq1 = vmull_u16(vget_high_u16(hi), vget_high_u16(hi));

    uint32x4_t sq32 = vaddq_u32(loSq0, loSq1);
    sq32 = vaddq_u32(sq32, hiSq0);
    sq32 = vaddq_u32(sq32, hiSq1);
    sumSq64 = vaddq_u64(sumSq64, vpaddlq_u32(sq32));
  }

  result.sum += horizontalAddU32(sum32);
  result.sumSq += horizontalAddU64(sumSq64);

  const uint8_t* tail = ptr + offsetBytes;
  for (; i < samples; ++i, tail += xStep) {
    const uint8_t value = *tail;
    result.sum += static_cast<uint64_t>(value);
    result.sumSq += static_cast<uint64_t>(value) * static_cast<uint64_t>(value);
  }
  return result;
}

inline SumResult sumRowStride1Step4(const uint8_t* ptr, int samples, int xStep) {
  SumResult result{0, 0};
  if (samples <= 0) {
    return result;
  }
  const int limit = samples & ~15;
  uint32x4_t sum32 = vdupq_n_u32(0);
  uint64x2_t sumSq64 = vdupq_n_u64(0);

  int offsetBytes = 0;
  int i = 0;
  for (; i < limit; i += 16, offsetBytes += 64) {
    const uint8x16x4_t vec = vld4q_u8(ptr + offsetBytes);
    const uint8x16_t channel = vec.val[0];
    const uint16x8_t lo = vmovl_u8(vget_low_u8(channel));
    const uint16x8_t hi = vmovl_u8(vget_high_u8(channel));

    sum32 = vaddq_u32(sum32, vpaddlq_u16(lo));
    sum32 = vaddq_u32(sum32, vpaddlq_u16(hi));

    const uint32x4_t loSq0 = vmull_u16(vget_low_u16(lo), vget_low_u16(lo));
    const uint32x4_t loSq1 = vmull_u16(vget_high_u16(lo), vget_high_u16(lo));
    const uint32x4_t hiSq0 = vmull_u16(vget_low_u16(hi), vget_low_u16(hi));
    const uint32x4_t hiSq1 = vmull_u16(vget_high_u16(hi), vget_high_u16(hi));

    uint32x4_t sq32 = vaddq_u32(loSq0, loSq1);
    sq32 = vaddq_u32(sq32, hiSq0);
    sq32 = vaddq_u32(sq32, hiSq1);
    sumSq64 = vaddq_u64(sumSq64, vpaddlq_u32(sq32));
  }

  result.sum += horizontalAddU32(sum32);
  result.sumSq += horizontalAddU64(sumSq64);

  const uint8_t* tail = ptr + offsetBytes;
  for (; i < samples; ++i, tail += xStep) {
    const uint8_t value = *tail;
    result.sum += static_cast<uint64_t>(value);
    result.sumSq += static_cast<uint64_t>(value) * static_cast<uint64_t>(value);
  }
  return result;
}

inline SumResult sumRowStride4Step1(const uint8_t* rowPtr, int samples, int pixelStride, int channelOffset) {
  SumResult result{0, 0};
  if (samples <= 0) {
    return result;
  }
  const int limit = samples & ~15;
  uint32x4_t sum32 = vdupq_n_u32(0);
  uint64x2_t sumSq64 = vdupq_n_u64(0);

  int offsetBytes = 0;
  for (int i = 0; i < limit; i += 16, offsetBytes += pixelStride * 16) {
    const uint8x16x4_t vec = vld4q_u8(rowPtr + offsetBytes);
    const uint8x16_t channel = vec.val[channelOffset & 3];
    const uint16x8_t lo = vmovl_u8(vget_low_u8(channel));
    const uint16x8_t hi = vmovl_u8(vget_high_u8(channel));

    sum32 = vaddq_u32(sum32, vpaddlq_u16(lo));
    sum32 = vaddq_u32(sum32, vpaddlq_u16(hi));

    const uint32x4_t loSq0 = vmull_u16(vget_low_u16(lo), vget_low_u16(lo));
    const uint32x4_t loSq1 = vmull_u16(vget_high_u16(lo), vget_high_u16(lo));
    const uint32x4_t hiSq0 = vmull_u16(vget_low_u16(hi), vget_low_u16(hi));
    const uint32x4_t hiSq1 = vmull_u16(vget_high_u16(hi), vget_high_u16(hi));

    uint32x4_t sq32 = vaddq_u32(loSq0, loSq1);
    sq32 = vaddq_u32(sq32, hiSq0);
    sq32 = vaddq_u32(sq32, hiSq1);
    sumSq64 = vaddq_u64(sumSq64, vpaddlq_u32(sq32));
  }

  result.sum += horizontalAddU32(sum32);
  result.sumSq += horizontalAddU64(sumSq64);

  const uint8_t* tail = rowPtr + offsetBytes;
  for (int i = limit; i < samples; ++i) {
    const uint8_t value = tail[channelOffset];
    result.sum += static_cast<uint64_t>(value);
    result.sumSq += static_cast<uint64_t>(value) * static_cast<uint64_t>(value);
    tail += pixelStride;
  }
  return result;
}
#endif  // HAVE_NEON

inline SumResult sumRow(
    const uint8_t* rowPtr,
    int samples,
    int pixelStride,
    int channelOffset,
    int xStep) {
#if HAVE_NEON
  if (pixelStride == 1) {
    const uint8_t* ptr = rowPtr + channelOffset;
    switch (xStep) {
      case 1:
        return sumRowStride1Step1(ptr, samples);
      case 2:
        return sumRowStride1Step2(ptr, samples, xStep);
      case 4:
        return sumRowStride1Step4(ptr, samples, xStep);
      default:
        break;
    }
  } else if (pixelStride == 4 && xStep == 1 && channelOffset >= 0 && channelOffset < 4) {
    return sumRowStride4Step1(rowPtr, samples, pixelStride, channelOffset);
  }
#endif
  return sumRowScalar(rowPtr, samples, pixelStride, channelOffset, xStep);
}

}  // namespace

extern "C" JNIEXPORT jdoubleArray JNICALL
Java_com_heartpyapp_ppg_PPGMeanPlugin_nativeSumAndSquares(
    JNIEnv* env,
    jclass /*clazz*/,
    jobject buffer,
    jint baseOffset,
    jint bytesPerRow,
    jint roiWidth,
    jint roiHeight,
    jint pixelStride,
    jint channelOffset,
    jint xStep,
    jint yStep) {
  if (buffer == nullptr) {
    return nullptr;
  }
  if (roiWidth <= 0 || roiHeight <= 0 || bytesPerRow <= 0 || pixelStride <= 0 ||
      xStep <= 0 || yStep <= 0) {
    return nullptr;
  }
  auto* basePtr = static_cast<uint8_t*>(env->GetDirectBufferAddress(buffer));
  if (basePtr == nullptr) {
    return nullptr;
  }

  const int sampleCols = computeSampleCount(roiWidth, xStep);
  const int sampleRows = computeSampleCount(roiHeight, yStep);
  if (sampleCols <= 0 || sampleRows <= 0) {
    return nullptr;
  }

  const uint8_t* roiBase = basePtr + baseOffset;
  uint64_t totalSum = 0;
  uint64_t totalSumSq = 0;

  int currentY = 0;
  for (int rowIndex = 0; rowIndex < sampleRows; ++rowIndex, currentY += yStep) {
    const uint8_t* rowPtr = roiBase + currentY * bytesPerRow;
    const SumResult row = sumRow(rowPtr, sampleCols, pixelStride, channelOffset, xStep);
    totalSum += row.sum;
    totalSumSq += row.sumSq;
  }

  jdoubleArray out = env->NewDoubleArray(2);
  if (out == nullptr) {
    __android_log_print(ANDROID_LOG_ERROR, kTag, "Failed to allocate result array");
    return nullptr;
  }
  const jdouble values[2] = {
      static_cast<jdouble>(totalSum),
      static_cast<jdouble>(totalSumSq),
  };
  env->SetDoubleArrayRegion(out, 0, 2, values);
  return out;
}
