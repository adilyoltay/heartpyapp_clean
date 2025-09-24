#include "heartpy_core.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <complex>
#include <stdexcept>
#include <string>
#include <sstream>
#include <atomic>
#include <cstdio>
#include <cstdarg>
#include <mutex>
#include <unordered_map>
#if defined(__ANDROID__)
#include <android/log.h>
#endif
#if defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE || TARGET_OS_SIMULATOR
#include <os/log.h>
#endif
#endif
#ifdef USE_ACCELERATE_FFT
#include <Accelerate/Accelerate.h>
#endif
#if defined(HEARTPY_ENABLE_NEON) && defined(__ARM_NEON)
#include <arm_neon.h>
#endif

namespace heartpy {

static bool s_deterministic = false;

namespace {

static constexpr double PI = 3.141592653589793238462643383279502884;

static std::atomic<unsigned long long> g_welchGuardFallbackCount{0};
static std::atomic<unsigned long long> g_welchGuardFailureCount{0};

static void logWelchGuard(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    std::fprintf(stderr, "[HeartPySNR][welchPSD] ");
    std::vfprintf(stderr, fmt, args);
    std::fprintf(stderr, "\n");
    std::fflush(stderr);
    va_end(args);
}

static void logAnalyze(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
#if defined(__ANDROID__)
    __android_log_vprint(ANDROID_LOG_DEBUG, "HeartPyAnalyze", fmt, args);
#elif defined(__APPLE__)
#if TARGET_OS_IPHONE || TARGET_OS_SIMULATOR
    char buffer[512];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    os_log_with_type(OS_LOG_DEFAULT, OS_LOG_TYPE_DEBUG, "[HeartPyAnalyze] %{public}s", buffer);
#else
    std::fprintf(stderr, "[HeartPyAnalyze] ");
    std::vfprintf(stderr, fmt, args);
    std::fprintf(stderr, "\n");
    std::fflush(stderr);
#endif
#else
    std::fprintf(stderr, "[HeartPyAnalyze] ");
    std::vfprintf(stderr, fmt, args);
    std::fprintf(stderr, "\n");
    std::fflush(stderr);
#endif
    va_end(args);
}

#if defined(USE_ACCELERATE_FFT)
class FFTSetupCache {
public:
    FFTSetupD acquire(int nfft) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_.find(nfft);
        if (it != cache_.end()) {
            return it->second;
        }
        FFTSetupD setup = vDSP_create_fftsetupD(
            static_cast<vDSP_Length>(std::log2(nfft)),
            kFFTRadix2);
        cache_[nfft] = setup;
        logWelchGuard("Created FFTSetupD cache entry (nfft=%d)", nfft);
        return setup;
    }

    ~FFTSetupCache() {
        for (auto& entry : cache_) {
            if (entry.second) {
                vDSP_destroy_fftsetupD(entry.second);
            }
        }
    }

private:
    std::mutex mutex_;
    std::unordered_map<int, FFTSetupD> cache_;
};

static FFTSetupCache& getFFTSetupCache() {
    static FFTSetupCache cache;
    return cache;
}
#elif defined(USE_KISSFFT)
class KissFftCache {
public:
    kiss_fftr_cfg acquire(int nfft) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto [it, inserted] = cache_.emplace(nfft, nullptr);
        if (inserted || it->second == nullptr) {
            it->second = kiss_fftr_alloc(nfft, 0, nullptr, nullptr);
            logWelchGuard("Created kiss_fftr_cfg cache entry (nfft=%d)", nfft);
        }
        return it->second;
    }

    ~KissFftCache() {
        for (auto& entry : cache_) {
            if (entry.second) {
                kiss_fftr_free(entry.second);
            }
        }
    }

private:
    std::mutex mutex_;
    std::unordered_map<int, kiss_fftr_cfg> cache_;
};

static KissFftCache& getKissFftCache() {
    static KissFftCache cache;
    return cache;
}
#endif

// fwd decl
static std::vector<int> quotientFilterMask(const std::vector<double>& rr, const std::vector<int>& base_mask, int iterations = 2);

static inline double clamp(double v, double lo, double hi) {
	return std::max(lo, std::min(hi, v));
}
// Round to 1e-6 precision to match HeartPy float behavior in threshold comparisons
static inline double round6(double x) { return std::round(x * 1e6) / 1e6; }

// Simple moving average detrend
std::vector<double> movingAverageDetrend(const std::vector<double>& x, int window) {
	if (window <= 1) return x;
	const int n = static_cast<int>(x.size());
	std::vector<double> out(n);
	std::vector<double> cumsum(n + 1, 0.0);
	for (int i = 0; i < n; ++i) cumsum[i + 1] = cumsum[i] + x[i];
	for (int i = 0; i < n; ++i) {
		int start = std::max(0, i - window / 2);
		int end = std::min(n, i + (window - window / 2));
		double mean = (cumsum[end] - cumsum[start]) / std::max(1, end - start);
		out[i] = x[i] - mean;
	}
	return out;
}

// Biquad IIR bandpass (RBJ cookbook)
struct Biquad {
	double b0{0}, b1{0}, b2{0}, a1{0}, a2{0};
	double z1{0}, z2{0};

	double process(double in) {
		double out = in * b0 + z1;
		z1 = in * b1 + z2 - a1 * out;
		z2 = in * b2 - a2 * out;
		return out;
	}
};

Biquad designBandpass(double fs, double f0, double Q) {
	const double w0 = 2.0 * PI * f0 / fs;
	const double alpha = std::sin(w0) / (2.0 * Q);
	const double cosw0 = std::cos(w0);

	Biquad bi;
	double b0 =   alpha;
	double b1 =   0.0;
	double b2 =  -alpha;
	double a0 =   1.0 + alpha;
	double a1 =  -2.0 * cosw0;
	double a2 =   1.0 - alpha;

	bi.b0 = b0 / a0;
	bi.b1 = b1 / a0;
	bi.b2 = b2 / a0;
	bi.a1 = a1 / a0;
	bi.a2 = a2 / a0;
	return bi;
}

std::vector<double> bandpassFilter(const std::vector<double>& x, double fs, double lowHz, double highHz, int order) {
	if (lowHz <= 0.0 && highHz <= 0.0) return x;
	const int n = static_cast<int>(x.size());
	std::vector<double> y = x;
	// Cascade bandpass sections across center freqs between low-high
	const int sections = std::max(1, order);
	for (int s = 0; s < sections; ++s) {
		double f0 = lowHz + (highHz - lowHz) * (s + 0.5) / sections;
		double bw = (highHz - lowHz);
		double Q = (bw > 0.0 && f0 > 0.0) ? f0 / bw : 0.707;
		Biquad bi = designBandpass(fs, clamp(f0, 0.001, fs * 0.45), std::max(0.2, Q));
		double z1 = 0.0, z2 = 0.0; (void)z1; (void)z2;
		for (int i = 0; i < n; ++i) y[i] = bi.process(y[i]);
	}
	return y;
}

// Adaptive threshold peak detection
std::vector<int> detectPeaks(const std::vector<double>& x, double fs, double refractoryMs, double scale) {
	const int n = static_cast<int>(x.size());
	std::vector<int> peaks;
	if (n == 0) return peaks;
	const int refSamples = static_cast<int>(std::round(refractoryMs * 0.001 * fs));
	// compute rolling mean and std via simple window
	const int win = std::max(5, static_cast<int>(std::round(0.5 * fs)));
	std::vector<double> cumsum(n + 1, 0.0), csumsq(n + 1, 0.0);
	for (int i = 0; i < n; ++i) {
		cumsum[i + 1] = cumsum[i] + x[i];
		csumsq[i + 1] = csumsq[i] + x[i] * x[i];
	}
	int lastPeak = -refSamples - 1;
	for (int i = 1; i < n - 1; ++i) {
		int start = std::max(0, i - win);
		int end = std::min(n, i + win);
		int count = std::max(1, end - start);
		double mean = (cumsum[end] - cumsum[start]) / count;
		double var = (csumsq[end] - csumsq[start]) / count - mean * mean;
		double sd = std::sqrt(std::max(0.0, var));
		double thr = mean + scale * sd;
		bool isPeak = (x[i] > thr) && (x[i] > x[i - 1]) && (x[i] >= x[i + 1]);
		if (isPeak && (i - lastPeak >= refSamples)) {
			peaks.push_back(i);
			lastPeak = i;
		}
	}
	return peaks;
}

// Utility stats
double mean(const std::vector<double>& v) {
	if (v.empty()) return 0.0;
	double s = std::accumulate(v.begin(), v.end(), 0.0);
	return s / static_cast<double>(v.size());
}

// HeartPy-style quotient filter: builds/updates a mask (0=accept,1=reject)
static std::vector<int> quotientFilterMask(const std::vector<double>& rr, const std::vector<int>& base_mask, int iterations) {
    const size_t n = rr.size();
    std::vector<int> mask;
    if (base_mask.empty()) mask.assign(n, 0); else mask = base_mask;
    for (int it = 0; it < iterations; ++it) {
        if (n < 2) break;
        for (size_t i = 0; i + 1 < n; ++i) {
            if (mask[i] + mask[i + 1] != 0) continue; // skip if any already rejected
            double r1 = rr[i];
            double r2 = rr[i + 1];
            if (r2 == 0.0) { mask[i] = 1; continue; }
            double q = r1 / r2;
            if (q < 0.8 || q > 1.2) {
                mask[i] = 1; // mark current, leave i+1 as in HP
            }
        }
    }
    return mask;
}
double sd(const std::vector<double>& v) {
	if (v.size() <= 1) return 0.0;
	double m = mean(v);
	double acc = 0.0;
	for (double x : v) {
		double d = x - m;
		acc += d * d;
	}
	return std::sqrt(acc / static_cast<double>(v.size() - 1));
}

// Welch PSD (density), Hann window, one-sided, SciPy-like normalization
struct PSDResult { std::vector<double> freqs; std::vector<double> psd; };

static inline bool isPowerOfTwo(int x) { return x > 0 && (x & (x - 1)) == 0; }

static void fft_inplace(std::vector<std::complex<double>>& a) {
    const size_t n = a.size();
    if (n <= 1) return;
    // bit-reversal
    size_t j = 0;
    for (size_t i = 1; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    // Cooley-Tukey
    for (size_t len = 2; len <= n; len <<= 1) {
        double ang = -2.0 * PI / static_cast<double>(len);
        std::complex<double> wlen(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> w(1.0, 0.0);
            for (size_t j2 = 0; j2 < len / 2; ++j2) {
                std::complex<double> u = a[i + j2];
                std::complex<double> v = a[i + j2 + len / 2] * w;
                a[i + j2] = u + v;
                a[i + j2 + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

PSDResult welchPSD(const std::vector<double>& x, double fs, int nfft, double overlap) {
    const int n = static_cast<int>(x.size());
    if (nfft <= 0) nfft = 256;
    overlap = clamp(overlap, 0.0, 0.95);

    constexpr int kMinNfft = 32;
    const int originalNfft = nfft;
    const double originalOverlap = overlap;

    auto largestPowerOfTwoLE = [](int value) -> int {
        if (value < 1) return 0;
        int pow2 = 1;
        while ((pow2 << 1) <= value && (pow2 << 1) > 0) {
            pow2 <<= 1;
        }
        return pow2;
    };

    int workingNfft = std::max(kMinNfft, nfft);
    double workingOverlap = overlap;
    int step = 1;
    int nseg = 0;
    bool paramsReady = false;
    bool adjustmentOccurred = false;

    while (workingNfft >= kMinNfft) {
        if (n < workingNfft) {
            int nextNfft = largestPowerOfTwoLE(n);
            if (nextNfft < kMinNfft) {
                break;
            }
            if (nextNfft != workingNfft) {
                logWelchGuard("Signal shorter than nfft (%d < %d). Reducing nfft to %d", n, workingNfft, nextNfft);
                adjustmentOccurred = true;
                workingNfft = nextNfft;
                continue;
            }
        }

        if (n <= workingNfft) {
            // Even with maximum overlap we cannot form >=2 segments; shrink nfft further
            if (workingNfft == kMinNfft) {
                break;
            }
            int nextNfft = largestPowerOfTwoLE(workingNfft - 1);
            if (nextNfft < kMinNfft) {
                break;
            }
            logWelchGuard("Insufficient signal span for nfft=%d (n=%d). Reducing to %d", workingNfft, n, nextNfft);
            adjustmentOccurred = true;
            workingNfft = nextNfft;
            continue;
        }

        double minOverlapForTwo = 1.0 - static_cast<double>(n - workingNfft) / static_cast<double>(workingNfft);
        minOverlapForTwo = clamp(minOverlapForTwo, 0.0, 0.95);
        double candidateOverlap = std::max(workingOverlap, minOverlapForTwo + 0.02);
        candidateOverlap = clamp(candidateOverlap, 0.0, 0.95);

        double stepFloat = static_cast<double>(workingNfft) * (1.0 - candidateOverlap);
        if (stepFloat < 1.0) stepFloat = 1.0;
        step = std::max(1, static_cast<int>(std::round(stepFloat)));
        nseg = 1 + (n - workingNfft) / step;

        if (nseg >= 2) {
            if (std::fabs(candidateOverlap - workingOverlap) > 1e-6) {
                adjustmentOccurred = true;
            }
            workingOverlap = candidateOverlap;
            paramsReady = true;
            break;
        }

        if (candidateOverlap < 0.95 - 1e-6) {
            workingOverlap = std::min(0.95, candidateOverlap + 0.05);
            adjustmentOccurred = true;
            continue;
        }

        if (workingNfft == kMinNfft) {
            break;
        }
        int nextNfft = largestPowerOfTwoLE(workingNfft - 1);
        if (nextNfft < kMinNfft) {
            break;
        }
        logWelchGuard("Rounding prevented nseg>=2 for nfft=%d (n=%d). Reducing to %d", workingNfft, n, nextNfft);
        adjustmentOccurred = true;
        workingNfft = nextNfft;
    }

    if (!paramsReady) {
        g_welchGuardFailureCount.fetch_add(1);
        logWelchGuard("Unable to satisfy Welch params (n=%d, requested nfft=%d)", n, originalNfft);
        return {{}, {}};
    }

    if (adjustmentOccurred) {
        g_welchGuardFallbackCount.fetch_add(1);
        logWelchGuard("Adjusted Welch params: nfft %d -> %d, overlap %.3f -> %.3f, nseg=%d, n=%d", originalNfft, workingNfft, originalOverlap, workingOverlap, nseg, n);
    }

    // Enforce a lower bound on usable nfft for PSD stability
    constexpr int kWelchMinimumUsableNfft = 64;
    if (workingNfft < kWelchMinimumUsableNfft) {
        g_welchGuardFailureCount.fetch_add(1);
        logWelchGuard("Rejecting Welch params: nfft=%d < %d (n=%d)", workingNfft, kWelchMinimumUsableNfft, n);
        return {{}, {}};
    }

    nfft = workingNfft;
    overlap = workingOverlap;

    // Hann window
    std::vector<double> w(nfft);
    for (int i = 0; i < nfft; ++i) w[i] = 0.5 - 0.5 * std::cos(2.0 * PI * i / (nfft - 1));
    double U = 0.0;
#if defined(HEARTPY_ENABLE_ACCELERATE)
    // Use vDSP to compute sum of squares when enabled
    vDSP_svesqD(w.data(), 1, &U, (vDSP_Length)nfft);
#else
    for (double v : w) U += v * v; // sum(w^2)
#endif

    const int kmax = nfft / 2 + 1;
    std::vector<double> P(kmax, 0.0);

    bool useFFT = isPowerOfTwo(nfft);
    if (heartpy::isDeterministic()) useFFT = false; // force DFT for determinism
    if (useFFT) {
#ifdef USE_ACCELERATE_FFT
        // Use Accelerate vDSP double-precision split-complex FFT if available
        FFTSetupD setup = getFFTSetupCache().acquire(nfft);
        std::vector<double> real(nfft), imag(nfft, 0.0);
        DSPDoubleSplitComplex split{real.data(), imag.data()};
        for (int s = 0; s < nseg; ++s) {
            int start = s * step;
            // Copy segment into real buffer
            std::memcpy(real.data(), &x[start], sizeof(double) * (size_t)nfft);
#if defined(HEARTPY_ENABLE_ACCELERATE)
            // mu = mean(real)
            double mu = 0.0; vDSP_meanvD(real.data(), 1, &mu, (vDSP_Length)nfft);
            // real = (real - mu)
            double negMu = -mu; vDSP_vsaddD(real.data(), 1, &negMu, real.data(), 1, (vDSP_Length)nfft);
            // real = real .* w
            vDSP_vmulD(real.data(), 1, w.data(), 1, real.data(), 1, (vDSP_Length)nfft);
#else
            // Scalar detrend + window (fallback)
            double mu = 0.0; for (int t = 0; t < nfft; ++t) mu += real[t]; mu /= nfft;
            for (int t = 0; t < nfft; ++t) real[t] = (real[t] - mu) * w[t];
#endif
            vDSP_fft_zipD(setup, &split, 1, static_cast<vDSP_Length>(std::log2(nfft)), kFFTDirection_Forward);
            for (int k = 0; k < kmax; ++k) {
                double realv = real[k];
                double imagv = imag[k];
                double Sxx = realv * realv + imagv * imagv;
                double Pseg = Sxx / (fs * U);
                P[k] += Pseg;
            }
        }
#elif defined(USE_KISSFFT)
        kiss_fftr_cfg cfg = getKissFftCache().acquire(nfft);
        std::vector<float> in(nfft);
        std::vector<kiss_fft_cpx> out(kmax);
        for (int s = 0; s < nseg; ++s) {
            int start = s * step;
            // detrend (constant) and window
#if defined(HEARTPY_ENABLE_NEON) && defined(__ARM_NEON)
            // Compute mean using NEON reduction in float
            float32x4_t acc4 = vdupq_n_f32(0.0f);
            int t_mean = 0;
            for (; t_mean + 4 <= nfft; t_mean += 4) {
                float32x4_t xv = { (float)x[start + t_mean + 0], (float)x[start + t_mean + 1], (float)x[start + t_mean + 2], (float)x[start + t_mean + 3] };
                acc4 = vaddq_f32(acc4, xv);
            }
            float acc = vgetq_lane_f32(acc4, 0) + vgetq_lane_f32(acc4, 1) + vgetq_lane_f32(acc4, 2) + vgetq_lane_f32(acc4, 3);
            for (; t_mean < nfft; ++t_mean) acc += (float)x[start + t_mean];
            const float fmu = acc / (float)nfft;
            int t = 0;
            for (; t + 4 <= nfft; t += 4) {
                float32x4_t xv = { (float)x[start + t + 0], (float)x[start + t + 1], (float)x[start + t + 2], (float)x[start + t + 3] };
                float32x4_t wv = { (float)w[t + 0], (float)w[t + 1], (float)w[t + 2], (float)w[t + 3] };
                float32x4_t mu4 = vdupq_n_f32(fmu);
                float32x4_t dv = vsubq_f32(xv, mu4);
                float32x4_t yv = vmulq_f32(dv, wv);
                vst1q_f32(&in[t], yv);
            }
            for (; t < nfft; ++t) in[t] = ((float)x[start + t] - fmu) * (float)w[t];
#else
            double mu = 0.0; for (int t = 0; t < nfft; ++t) mu += x[start + t]; mu /= nfft;
            for (int t = 0; t < nfft; ++t) in[t] = static_cast<float>((x[start + t] - mu) * w[t]);
#endif
            kiss_fftr(cfg, in.data(), out.data());
            for (int k = 0; k < kmax; ++k) {
                double realv = out[k].r;
                double imagv = out[k].i;
                double Sxx = realv * realv + imagv * imagv;
                double Pseg = Sxx / (fs * U);
                P[k] += Pseg;
            }
        }
#else
        std::vector<std::complex<double>> buf(nfft);
        for (int s = 0; s < nseg; ++s) {
            int start = s * step;
            // detrend (constant)
            double mu = 0.0; for (int t = 0; t < nfft; ++t) mu += x[start + t]; mu /= nfft;
            for (int t = 0; t < nfft; ++t) buf[t] = std::complex<double>((x[start + t] - mu) * w[t], 0.0);
            fft_inplace(buf);
            for (int k = 0; k < kmax; ++k) {
                double real = buf[k].real();
                double imag = buf[k].imag();
                double Sxx = real * real + imag * imag;
                double Pseg = Sxx / (fs * U);
                P[k] += Pseg;
            }
        }
#endif
    } else {
        // fallback to naive DFT
        for (int s = 0; s < nseg; ++s) {
            int start = s * step;
            for (int k = 0; k < kmax; ++k) {
                double real = 0.0, imag = 0.0;
                for (int t = 0; t < nfft; ++t) {
                    double sample = x[start + t] * w[t];
                    double ang = -2.0 * PI * k * t / nfft;
                    real += sample * std::cos(ang);
                    imag += sample * std::sin(ang);
                }
                double Sxx = real * real + imag * imag;
                double Pseg = Sxx / (fs * U);
                P[k] += Pseg;
            }
        }
    }
    for (double& v : P) v /= static_cast<double>(nseg);
    // one-sided correction (DC and Nyquist untouched)
    if (kmax > 1) {
        int last = (nfft % 2 == 0) ? (kmax - 1) : kmax;
        for (int k = 1; k < last; ++k) P[k] *= 2.0;
    }
    std::vector<double> freqs(kmax);
    for (int k = 0; k < kmax; ++k) freqs[k] = (fs * k) / nfft;
    return {freqs, P};
}

// HeartPy-style band integration: select bins fully inside band and apply trapz with constant dx
double integrateBand(const std::vector<double>& f, const std::vector<double>& p, double lo, double hi) {
    if (f.size() < 2 || p.size() != f.size()) return 0.0;
    // constant spacing assumed by our welchPSD
    double df = f[1] - f[0];
    std::vector<double> vals;
    vals.reserve(p.size());
    for (size_t i = 0; i < f.size(); ++i) if (f[i] >= lo && f[i] < hi) vals.push_back(std::abs(p[i]));
    if (vals.size() < 2) return 0.0;
    double area = 0.0;
    for (size_t i = 1; i < vals.size(); ++i) area += 0.5 * (vals[i-1] + vals[i]) * df;
    return area;
}
    
// Helper: enforce refractory by keeping strongest peak in conflicts
static std::vector<int> enforceRefractory(const std::vector<double>& x, const std::vector<int>& peaks, int refSamples) {
    if (peaks.empty()) return peaks;
    std::vector<int> out;
    out.reserve(peaks.size());
    int i = 0;
    while (i < static_cast<int>(peaks.size())) {
        int j = i + 1;
        int best = peaks[i];
        double bestVal = x[best];
        while (j < static_cast<int>(peaks.size()) && (peaks[j] - peaks[i]) < refSamples) {
            if (x[peaks[j]] > bestVal) { best = peaks[j]; bestVal = x[best]; }
            ++j;
        }
        out.push_back(best);
        int next = j;
        while (next < static_cast<int>(peaks.size()) && (peaks[next] - best) < refSamples) ++next;
        i = next;
    }
    return out;
}

// Natural cubic spline for 1D interpolation (no smoothing)
struct CubicSpline {
    std::vector<double> x, a, b, c, d; // a=y
    bool ok{false};
};

CubicSpline buildNaturalCubic(const std::vector<double>& xs, const std::vector<double>& ys) {
    CubicSpline sp; sp.x = xs; sp.a = ys;
    int n = static_cast<int>(xs.size());
    if (n < 3) { sp.ok = false; return sp; }
    std::vector<double> h(n-1);
    for (int i=0;i<n-1;++i) h[i] = xs[i+1]-xs[i];
    std::vector<double> alpha(n); alpha[0]=0; alpha[n-1]=0;
    for (int i=1;i<n-1;++i) {
        alpha[i] = 3.0*((ys[i+1]-ys[i])/h[i] - (ys[i]-ys[i-1])/h[i-1]);
    }
    std::vector<double> l(n), mu(n), z(n);
    l[0]=1; mu[0]=0; z[0]=0;
    for (int i=1;i<n-1;++i) {
        l[i] = 2.0*(xs[i+1]-xs[i-1]) - h[i-1]*mu[i-1];
        mu[i] = h[i]/l[i];
        z[i] = (alpha[i]-h[i-1]*z[i-1])/l[i];
    }
    l[n-1]=1; z[n-1]=0; std::vector<double> c(n), b(n-1), d(n-1);
    c[n-1]=0;
    for (int j=n-2;j>=0;--j) {
        c[j] = z[j] - mu[j]*c[j+1];
        b[j] = (ys[j+1]-ys[j])/h[j] - h[j]*(c[j+1]+2.0*c[j])/3.0;
        d[j] = (c[j+1]-c[j])/(3.0*h[j]);
    }
    sp.b=b; sp.c=c; sp.d=d; sp.ok=true; return sp;
}

static std::vector<double> boxcarSmooth(const std::vector<double>& y, int win) {
    if (win <= 1 || y.empty()) return y;
    int n = static_cast<int>(y.size());
    std::vector<double> out(n);
    int hw = win / 2;
    for (int i = 0; i < n; ++i) {
        int a = std::max(0, i - hw);
        int b = std::min(n - 1, i + hw);
        double sum = 0.0; int cnt = 0;
        for (int j = a; j <= b; ++j) { sum += y[j]; ++cnt; }
        out[i] = sum / std::max(1, cnt);
    }
    return out;
}

// Apply A = I + lambda * L^T L to vector v, where L is second-difference operator
static void applySmoothingMatrix(const std::vector<double>& v, double lambda, std::vector<double>& out) {
    size_t n = v.size();
    out.assign(n, 0.0);
    if (n == 0) return;
    // u = L^T (L v)
    std::vector<double> u(n, 0.0);
    if (n >= 3) {
        for (size_t k = 0; k + 2 < n; ++k) {
            double w = v[k] - 2.0 * v[k + 1] + v[k + 2];
            u[k]     += w;
            u[k + 1] += -2.0 * w;
            u[k + 2] += w;
        }
    }
    for (size_t i = 0; i < n; ++i) out[i] = v[i] + lambda * u[i];
}

static std::vector<double> smoothRR_CG(const std::vector<double>& rr, double lambda, int max_iters = 200, double tol = 1e-6) {
    size_t n = rr.size();
    if (n < 3 || lambda <= 0.0) return rr;
    std::vector<double> x = rr; // initial guess
    std::vector<double> Ax(n), r(n), p(n), Ap(n);
    applySmoothingMatrix(x, lambda, Ax);
    for (size_t i = 0; i < n; ++i) r[i] = rr[i] - Ax[i];
    p = r;
    double rsold = 0.0; for (double ri : r) rsold += ri * ri;
    double bnorm = 0.0; for (double bi : rr) bnorm += bi * bi; bnorm = std::sqrt(std::max(1e-12, bnorm));
    for (int it = 0; it < max_iters; ++it) {
        applySmoothingMatrix(p, lambda, Ap);
        double pAp = 0.0; for (size_t i = 0; i < n; ++i) pAp += p[i] * Ap[i];
        if (std::fabs(pAp) < 1e-18) break;
        double alpha = rsold / pAp;
        for (size_t i = 0; i < n; ++i) x[i] += alpha * p[i];
        for (size_t i = 0; i < n; ++i) r[i] -= alpha * Ap[i];
        double rsnew = 0.0; for (double ri : r) rsnew += ri * ri;
        if (std::sqrt(rsnew) < tol * bnorm) break;
        double beta = rsnew / std::max(1e-18, rsold);
        for (size_t i = 0; i < n; ++i) p[i] = r[i] + beta * p[i];
        rsold = rsnew;
    }
    return x;
}

static std::vector<double> smoothRR_TargetSse(const std::vector<double>& rr, double target_sse) {
    if (rr.size() < 3 || target_sse <= 0.0) return rr;
    auto sse_for_lambda = [&](double lambda) {
        auto yhat = smoothRR_CG(rr, lambda);
        double sse = 0.0; for (size_t i = 0; i < rr.size(); ++i) { double d = yhat[i] - rr[i]; sse += d * d; }
        return std::pair<double, std::vector<double>>(sse, std::move(yhat));
    };
    // bracket lambda so that sse(lambda_high) >= target
    double lo = 0.0, hi = 1.0;
    auto p0 = sse_for_lambda(lo);
    if (p0.first >= target_sse) return p0.second; // already enough error (shouldn't happen)
    std::pair<double, std::vector<double>> phi;
    for (int k = 0; k < 40; ++k) { // expand hi exponentially
        phi = sse_for_lambda(hi);
        if (phi.first >= target_sse) break;
        hi *= 2.0;
        if (hi > 1e12) break;
    }
    std::vector<double> best = phi.second;
    // bisection
    for (int it = 0; it < 40; ++it) {
        double mid = (lo + hi) * 0.5;
        auto pm = sse_for_lambda(mid);
        best = pm.second;
        if (pm.first > target_sse) {
            hi = mid;
        } else {
            lo = mid;
        }
        if (std::fabs(pm.first - target_sse) / std::max(1.0, target_sse) < 1e-3) break;
    }
    return best;
}

double splineEval(const CubicSpline& sp, double xx) {
    int n = static_cast<int>(sp.x.size());
    if (!sp.ok || n<2) return 0.0;
    // binary search
    int lo=0, hi=n-1;
    if (xx <= sp.x.front()) {
        hi=1; lo=0;
    } else if (xx >= sp.x.back()) {
        lo=n-2; hi=n-1;
    } else {
        while (hi-lo>1) { int mid=(lo+hi)/2; if (sp.x[mid] > xx) hi=mid; else lo=mid; }
    }
    double dx = xx - sp.x[lo];
    return sp.a[lo] + sp.b[lo]*dx + sp.c[lo]*dx*dx + sp.d[lo]*dx*dx*dx;
}

// HeartPy-style rolling mean (0.75s window typical)
std::vector<double> rollingMeanHP(const std::vector<double>& data, double fs, double windowSeconds) {
    const int N = static_cast<int>(windowSeconds * fs);
    const int n = static_cast<int>(data.size());
    if (N <= 1 || n == 0 || N > n) {
        double m = mean(data);
        return std::vector<double>(n, m);
    }
    std::vector<double> rol; rol.reserve(n - N + 1);
    double s = 0.0;
    for (int i = 0; i < N; ++i) s += data[i];
    rol.push_back(s / N);
    for (int i = N; i < n; ++i) { s += data[i]; s -= data[i - N]; rol.push_back(s / N); }
    int n_miss = static_cast<int>(std::abs(n - static_cast<int>(rol.size())) / 2);
    std::vector<double> out; out.reserve(n);
    for (int i = 0; i < n_miss; ++i) out.push_back(rol.front());
    out.insert(out.end(), rol.begin(), rol.end());
    while (static_cast<int>(out.size()) < n) out.push_back(rol.back());
    if (static_cast<int>(out.size()) > n) out.resize(n);
    return out;
}

// HP detect_peaks using raised rolling mean and segment maxima
std::vector<int> detectPeaksHP(const std::vector<double>& x, const std::vector<double>& rol_mean, double ma_perc, double fs) {
    const int n = static_cast<int>(x.size());
    if (n == 0 || rol_mean.size() != x.size()) return {};
    double mn = (mean(rol_mean) / 100.0) * ma_perc;
    std::vector<double> thr(n);
    for (int i = 0; i < n; ++i) thr[i] = rol_mean[i] + mn;
    std::vector<int> maskIdx; maskIdx.reserve(n);
    for (int i = 0; i < n; ++i) if (x[i] > thr[i]) maskIdx.push_back(i);
    if (maskIdx.empty()) return {};
    std::vector<int> edges; edges.push_back(0);
    for (size_t i = 1; i < maskIdx.size(); ++i) if (maskIdx[i] - maskIdx[i-1] > 1) edges.push_back(static_cast<int>(i));
    edges.push_back(static_cast<int>(maskIdx.size()));
    std::vector<int> peaklist; peaklist.reserve(edges.size());
    for (size_t e = 0; e + 1 < edges.size(); ++e) {
        int a = edges[e], b = edges[e+1]; if (a >= b) continue;
        int best_idx = maskIdx[a]; double best_val = x[best_idx];
        for (int j = a + 1; j < b; ++j) { int idx = maskIdx[j]; if (x[idx] > best_val) { best_val = x[idx]; best_idx = idx; } }
        peaklist.push_back(best_idx);
    }
    if (!peaklist.empty()) {
        if (peaklist[0] <= static_cast<int>((fs / 1000.0) * 150.0)) peaklist.erase(peaklist.begin());
    }
    return peaklist;
}

// Population std (ddof=0) like numpy's default
double std_pop(const std::vector<double>& v) {
    if (v.empty()) return 0.0;
    double m = mean(v); double acc = 0.0; for (double x : v) { double d = x - m; acc += d * d; }
    return std::sqrt(acc / static_cast<double>(v.size()));
}

struct HPFitResult { std::vector<int> peaks; double best_ma{0}; double rrsd{0}; double bpm{0}; bool ok{false}; };

HPFitResult fitPeaksHP(const std::vector<double>& x, double fs, double bpmMin, double bpmMax) {
    std::vector<double> rmean = rollingMeanHP(x, fs, 0.75);
    int ma_list_vals[] = {5,10,15,20,25,30,40,50,60,70,80,90,100,110,120,150,200,300};
    HPFitResult out;
    double best_rrsd = std::numeric_limits<double>::infinity();
    for (int ma : ma_list_vals) {
        auto peaks = detectPeaksHP(x, rmean, static_cast<double>(ma), fs);
        double bpm = (x.empty()) ? 0.0 : (static_cast<double>(peaks.size()) / (static_cast<double>(x.size()) / fs)) * 60.0;
        std::vector<double> rr; rr.reserve(peaks.size() > 1 ? peaks.size()-1 : 0);
        for (size_t i = 1; i < peaks.size(); ++i) rr.push_back((peaks[i] - peaks[i-1]) * 1000.0 / fs);
        double rrsd = rr.empty() ? std::numeric_limits<double>::infinity() : std_pop(rr);
        if (rrsd > 0.1 && bpm >= bpmMin && bpm <= bpmMax) {
            if (rrsd < best_rrsd) { best_rrsd = rrsd; out.peaks = peaks; out.best_ma = ma; out.rrsd = rrsd; out.bpm = bpm; out.ok = true; }
        }
    }
    return out;
}

// Simplified adaptive threshold tuning to keep BPM in [bpmMin, bpmMax]
static std::vector<int> detectPeaksAdaptive(const std::vector<double>& x, double fs, double refractoryMs,
                                            double initScale, double bpmMin, double bpmMax) {
    double scale = initScale;
    const int refSamples = static_cast<int>(std::round(refractoryMs * 0.001 * fs));
    std::vector<int> best;
    for (int iter = 0; iter < 6; ++iter) {
        std::vector<int> p = detectPeaks(x, fs, refractoryMs, scale);
        p = enforceRefractory(x, p, refSamples);
        if (p.size() >= 2) {
            std::vector<double> ibis;
            ibis.reserve(p.size() - 1);
            for (size_t i = 1; i < p.size(); ++i) ibis.push_back((p[i] - p[i-1]) * 1000.0 / fs);
            double meanIbi = mean(ibis);
            double bpm = meanIbi > 1e-6 ? 60000.0 / meanIbi : 0.0;
            best = p;
            if (bpm > bpmMax) scale *= 1.25; else if (bpm < bpmMin) scale *= 0.8; else break;
        } else {
            scale *= 0.8;
        }
    }
    if (!best.empty()) return best;
    return enforceRefractory(x, detectPeaks(x, fs, refractoryMs, scale), refSamples);
}

} // namespace

// Public preprocessing functions (match header declarations) in heartpy namespace
std::vector<double> scaleData(const std::vector<double>& signal, double newMin, double newMax) {
    if (signal.empty()) return signal;
    auto minmax = std::minmax_element(signal.begin(), signal.end());
    double oldMin = *minmax.first;
    double oldMax = *minmax.second;
    double oldRange = oldMax - oldMin;
    if (oldRange < 1e-12) return signal;
    std::vector<double> scaled;
    scaled.reserve(signal.size());
    double newRange = newMax - newMin;
    for (double val : signal) {
        double normalized = (val - oldMin) / oldRange;
        scaled.push_back(newMin + normalized * newRange);
    }
    return scaled;
}

std::vector<double> interpolateClipping(const std::vector<double>& signal, double /*fs*/, double threshold) {
    std::vector<double> result = signal;
    std::vector<bool> clipped(signal.size(), false);
    for (size_t i = 0; i < signal.size(); ++i) {
        if (signal[i] >= threshold) clipped[i] = true;
    }
    for (size_t i = 0; i < signal.size(); ++i) {
        if (clipped[i]) {
            size_t start = i;
            while (i < signal.size() && clipped[i]) ++i;
            size_t end = i - 1;
            if (start > 0 && end < signal.size() - 1) {
                double startVal = signal[start - 1];
                double endVal = signal[end + 1];
                for (size_t j = start; j <= end; ++j) {
                    double t = static_cast<double>(j - start + 1) / (end - start + 2);
                    result[j] = startVal + t * (endVal - startVal);
                }
            }
        }
    }
    return result;
}

std::vector<double> hampelFilter(const std::vector<double>& signal, int windowSize, double threshold) {
    std::vector<double> result = signal;
    int halfWindow = windowSize / 2;
    for (size_t i = 0; i < signal.size(); ++i) {
        int start = std::max(0, static_cast<int>(i) - halfWindow);
        int end = std::min(static_cast<int>(signal.size() - 1), static_cast<int>(i) + halfWindow);
        std::vector<double> window;
        for (int j = start; j <= end; ++j) window.push_back(signal[j]);
        std::sort(window.begin(), window.end());
        double medianVal = window[window.size() / 2];
        std::vector<double> deviations;
        for (double val : window) deviations.push_back(std::abs(val - medianVal));
        std::sort(deviations.begin(), deviations.end());
        double mad = deviations[deviations.size() / 2];
        if (std::abs(signal[i] - medianVal) > threshold * mad) result[i] = medianVal;
    }
    return result;
}

std::vector<double> removeBaselineWander(const std::vector<double>& signal, double fs) {
    double cutoff = 0.5;
    double rc = 1.0 / (2.0 * PI * cutoff);
    double dt = 1.0 / fs;
    double alpha = dt / (rc + dt);
    std::vector<double> result(signal.size());
    if (signal.empty()) return result;
    result[0] = signal[0];
    for (size_t i = 1; i < signal.size(); ++i) {
        result[i] = alpha * (result[i - 1] + signal[i] - signal[i - 1]);
    }
    return result;
}

std::vector<double> enhancePeaks(const std::vector<double>& signal, double /*fs*/) {
    std::vector<double> result(signal.size());
    if (signal.size() < 3) return signal;
    result[0] = signal[0];
    result.back() = signal.back();
    for (size_t i = 1; i < signal.size() - 1; ++i) {
        double derivative = (signal[i + 1] - signal[i - 1]) / 2.0;
        result[i] = signal[i] + 0.1 * derivative;
    }
    return result;
}

template <typename T>
std::string vectorToString(const std::vector<T>& vec) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < vec.size(); ++i) {
        oss << vec[i];
        if (i < vec.size() - 1) {
            oss << ", ";
        }
    }
    oss << "]";
    return oss.str();
}

HeartMetrics analyzeSignal(const std::vector<double>& signal, double fs, const Options& opt) {

	if (signal.empty()) throw std::invalid_argument("signal is empty");
	if (fs <= 0.0) throw std::invalid_argument("fs must be > 0");

	HeartMetrics m;
	std::vector<double> processed = signal;

	// Preprocessing pipeline
	if (opt.interpClipping) {
		processed = interpolateClipping(processed, fs, opt.clippingThreshold);
	}
	
	if (opt.hampelCorrect) {
		processed = hampelFilter(processed, opt.hampelWindow, opt.hampelThreshold);
	}
	
	if (opt.removeBaselineWander) {
		processed = removeBaselineWander(processed, fs);
	}
	
	if (opt.enhancePeaks) {
		processed = enhancePeaks(processed, fs);
	}

	// Ensure positive baseline
	auto minMax = std::minmax_element(processed.begin(), processed.end());
	if (*minMax.first < 0) {
		double offset = std::abs(*minMax.first);
		std::transform(processed.begin(), processed.end(), processed.begin(),
					  [offset](double val) { return val + offset; });
	}

	logAnalyze("analyzeSignal: filtered signal size=%zu (fs=%.3f)", processed.size(), fs);

	// 1) Detrend for later spectral analysis
	int detrendWin = std::max(5, static_cast<int>(std::round(0.75 * fs)));
	std::vector<double> x = movingAverageDetrend(processed, detrendWin);

	// 2) Bandpass (used primarily for spectral analysis); peak detection will use processed
	// Modes: AUTO (legacy), RBJ biquad, or BUTTER_FILTFILT (zero‑phase via forward+reverse one‑pole cascades)
	{
		auto onePoleLP = [&](const std::vector<double>& s, double fc){
			double rc = 1.0 / (2.0 * PI * fc);
			double dt = 1.0 / fs;
			double alpha = dt / (rc + dt);
			std::vector<double> y(s.size()); if (s.empty()) return y; y[0] = s[0];
			for (size_t i = 1; i < s.size(); ++i) y[i] = y[i-1] + alpha * (s[i] - y[i-1]);
			return y;
		};
		auto onePoleHP = [&](const std::vector<double>& s, double fc){
			double rc = 1.0 / (2.0 * PI * fc);
			double dt = 1.0 / fs;
			double alpha = rc / (rc + dt);
			std::vector<double> y(s.size()); if (s.empty()) return y; y[0] = s[0];
			for (size_t i = 1; i < s.size(); ++i) y[i] = alpha * (y[i-1] + s[i] - s[i-1]);
			return y;
		};
		auto do_filtfilt = [&](std::vector<double> in, double lo, double hi, int order){
			order = std::max(1, order);
			for (int i = 0; i < order; ++i) in = onePoleHP(in, lo);
			for (int i = 0; i < order; ++i) in = onePoleLP(in, hi);
			std::reverse(in.begin(), in.end());
			for (int i = 0; i < order; ++i) in = onePoleHP(in, lo);
			for (int i = 0; i < order; ++i) in = onePoleLP(in, hi);
			std::reverse(in.begin(), in.end());
			return in;
		};
		double lo = std::max(0.0001, opt.lowHz);
		double hi = std::max(0.0001, opt.highHz);
		switch (opt.filterMode) {
			case Options::FilterMode::RBJ:
				x = bandpassFilter(x, fs, opt.lowHz, opt.highHz, opt.iirOrder);
				break;
			case Options::FilterMode::BUTTER_FILTFILT:
				x = do_filtfilt(x, lo, hi, opt.iirOrder);
				break;
			case Options::FilterMode::AUTO:
			default:
				if (opt.iirOrder >= 3) x = do_filtfilt(x, lo, hi, opt.iirOrder);
				else x = bandpassFilter(x, fs, opt.lowHz, opt.highHz, opt.iirOrder);
				break;
		}
	}

	// 3) Peak detection: HeartPy-style fit_peaks on scaled processed signal
	std::vector<double> procForPeaks = scaleData(processed, 0.0, 1024.0);
	// Use scaled signal directly for HeartPy-style detection (HP uses rolling mean threshold)
	HPFitResult hpfit = fitPeaksHP(procForPeaks, fs, opt.bpmMin, opt.bpmMax);
    std::vector<int> peaks = hpfit.ok ? hpfit.peaks
                                        : detectPeaksAdaptive(procForPeaks, fs, opt.refractoryMs, opt.thresholdScale, opt.bpmMin, opt.bpmMax);
    // Optional high-precision refinement by local interpolation on scaled signal
    if (opt.highPrecision && opt.highPrecisionFs > fs && !peaks.empty()) {
        peaks = interpolatePeaks(procForPeaks, peaks, fs, opt.highPrecisionFs);
    }
    m.peakList = peaks;
    m.peakListRaw = peaks; // capture raw peaks before cleaning
    // After peak detection (detectPeaksHP_local)
    logAnalyze("analyzeSignal: raw peaks detected=%zu (hpfit_ok=%d)", m.peakListRaw.size(), hpfit.ok ? 1 : 0);
    logAnalyze("analyzeSignal: raw peaks content: %s", vectorToString(m.peakListRaw).c_str());

	// Quality assessment
	m.quality = assessSignalQuality(x, peaks, fs);

    // 4) HeartPy-style check_peaks: remove RR outliers based on mean ± max(30%, 300ms)
	    if (peaks.size() >= 2) {
        std::vector<double> rr_raw;
        rr_raw.reserve(peaks.size() - 1);
        for (size_t i = 1; i < peaks.size(); ++i) rr_raw.push_back((peaks[i] - peaks[i - 1]) * 1000.0 / fs);
        logAnalyze("analyzeSignal: rr intervals raw (ms): %s", vectorToString(rr_raw).c_str());
        double mean_rr = mean(rr_raw);
        double rrPercent = clamp(opt.rrOutlierPercent, 0.0, 1.0);
        double percentDelta = mean_rr * rrPercent;
        double deltaMin = std::max(0.0, opt.rrOutlierMinMs);
        double deltaMax = std::max(deltaMin, opt.rrOutlierMaxMs > 0.0 ? opt.rrOutlierMaxMs : percentDelta);
        double rrDelta = clamp(percentDelta, deltaMin > 0.0 ? deltaMin : percentDelta, deltaMax);
        double lower = mean_rr - rrDelta;
        double upper = mean_rr + rrDelta;
        logAnalyze("analyzeSignal: rr bounds lower=%.3f upper=%.3f mean=%.3f delta=%.3f (percent=%.2f%%)",
                   lower, upper, mean_rr, rrDelta, rrPercent * 100.0);
        // indices to remove in peaklist are rr indices + 1
        std::vector<char> keep_peak(peaks.size(), 1);
        for (size_t i = 0; i < rr_raw.size(); ++i) {
            if (rr_raw[i] <= lower || rr_raw[i] >= upper) {
                size_t idx = i + 1; if (idx < keep_peak.size()) keep_peak[idx] = 0;
            }
        }
        size_t keepCount = 0;
        size_t rejectCount = 0;
        {
            std::vector<int> keepMask;
            keepMask.reserve(keep_peak.size());
            for (char v : keep_peak) {
                if (v) {
                    ++keepCount;
                } else {
                    ++rejectCount;
                }
                keepMask.push_back(static_cast<int>(v));
            }
            logAnalyze("analyzeSignal: keep mask after rr filter: %s", vectorToString(keepMask).c_str());
        }
        logAnalyze("analyzeSignal: rr filter keep_count=%zu reject_count=%zu", keepCount, rejectCount);
        {
            std::vector<std::string> decisions;
            decisions.reserve(peaks.size());
            for (size_t i = 0; i < peaks.size(); ++i) {
                std::ostringstream oss;
                oss << peaks[i] << (keep_peak[i] ? "@keep" : "@drop");
                decisions.push_back(oss.str());
            }
            logAnalyze("analyzeSignal: rr filter decisions: %s", vectorToString(decisions).c_str());
        }
        {
            std::vector<int> peakDiffSamples;
            peakDiffSamples.reserve(peaks.size() > 1 ? peaks.size() - 1 : 0);
            for (size_t i = 1; i < peaks.size(); ++i) {
                peakDiffSamples.push_back(peaks[i] - peaks[i - 1]);
            }
            logAnalyze("analyzeSignal: peak sample deltas: %s", vectorToString(peakDiffSamples).c_str());
        }
        // Segmentwise rejection (HeartPy check_binary_quality): non-overlapping windows of N beats
        if (opt.rejectSegmentwise) {
            const int segSize = std::max(1, opt.segmentRejectWindowBeats);
            const int stepBeats = std::max(1, static_cast<int>(std::round(segSize * (1.0 - clamp(opt.segmentRejectOverlap, 0.0, 0.99)))));
            size_t idx = 0;
            while (idx < keep_peak.size()) {
                size_t end = std::min(idx + segSize, keep_peak.size());
                int rejected = 0;
                for (size_t i = idx; i < end; ++i) if (!keep_peak[i]) ++rejected;
                bool accept = !(rejected > opt.segmentRejectMaxRejects);
                if (!accept) { for (size_t i = idx; i < end; ++i) keep_peak[i] = 0; }
                HeartMetrics::BinarySegment bs;
                bs.index = static_cast<int>(idx / segSize);
                bs.startBeat = static_cast<int>(idx);
                bs.endBeat = static_cast<int>(end);
                bs.totalBeats = static_cast<int>(end - idx);
                bs.rejectedBeats = rejected;
                bs.accepted = accept;
                m.binarySegments.push_back(bs);
                idx += static_cast<size_t>(stepBeats);
                if (idx >= keep_peak.size()) break;
            }
        }
        std::vector<int> peaks_cor; peaks_cor.reserve(peaks.size());
        std::vector<size_t> acceptedRawIndices; acceptedRawIndices.reserve(peaks.size());
        m.binaryPeakMask.clear(); m.binaryPeakMask.reserve(keep_peak.size());
        m.quality.rejectedIndices.clear();
        for (size_t i = 0; i < peaks.size(); ++i) {
            int accept = keep_peak[i] ? 1 : 0;
            m.binaryPeakMask.push_back(accept);
            if (accept) {
                peaks_cor.push_back(peaks[i]);
                acceptedRawIndices.push_back(i);
            } else {
                m.quality.rejectedIndices.push_back(static_cast<int>(i));
            }
        }

        std::vector<int> spacingRejectedRawIndices;
        std::vector<double> spacingRejectedDeltaMs;
        if (opt.minPeakDistanceMs > 0.0 && peaks_cor.size() > 1) {
            double spacingMs = opt.minPeakDistanceMs;
            int minSamples = static_cast<int>(std::ceil(spacingMs * fs / 1000.0));
            if (minSamples > 1) {
                std::vector<int> filteredPeaks;
                std::vector<size_t> filteredRawIndices;
                filteredPeaks.reserve(peaks_cor.size());
                filteredRawIndices.reserve(acceptedRawIndices.size());
                filteredPeaks.push_back(peaks_cor.front());
                filteredRawIndices.push_back(acceptedRawIndices.front());
                int lastSample = peaks_cor.front();
                for (size_t idx = 1; idx < peaks_cor.size(); ++idx) {
                    int sample = peaks_cor[idx];
                    size_t rawIdx = acceptedRawIndices[idx];
                    int deltaSamples = sample - lastSample;
                    double deltaMs = deltaSamples * 1000.0 / fs;
                    if (deltaSamples < minSamples) {
                        spacingRejectedRawIndices.push_back(static_cast<int>(rawIdx));
                        spacingRejectedDeltaMs.push_back(deltaMs);
                        keep_peak[rawIdx] = 0;
                        m.binaryPeakMask[rawIdx] = 0;
                        m.quality.rejectedIndices.push_back(static_cast<int>(rawIdx));
                        continue;
                    }
                    filteredPeaks.push_back(sample);
                    filteredRawIndices.push_back(rawIdx);
                    lastSample = sample;
                }
                if (!spacingRejectedRawIndices.empty()) {
                    logAnalyze("analyzeSignal: spacing filter min_ms=%.3f removed=%zu", spacingMs, spacingRejectedRawIndices.size());
                    logAnalyze("analyzeSignal: spacing rejected raw indices: %s", vectorToString(spacingRejectedRawIndices).c_str());
                    logAnalyze("analyzeSignal: spacing rejected delta (ms): %s", vectorToString(spacingRejectedDeltaMs).c_str());
                    peaks_cor = std::move(filteredPeaks);
                    acceptedRawIndices = std::move(filteredRawIndices);
                    std::vector<int> keepMaskUpdated;
                    keepMaskUpdated.reserve(keep_peak.size());
                    for (char v : keep_peak) keepMaskUpdated.push_back(static_cast<int>(v));
                    logAnalyze("analyzeSignal: keep mask after spacing: %s", vectorToString(keepMaskUpdated).c_str());
                }
            }
        }

        if (peaks_cor.size() > 1) {
            std::vector<int> peakDiffSamplesCor;
            peakDiffSamplesCor.reserve(peaks_cor.size() - 1);
            std::vector<double> peakDiffMsCor;
            peakDiffMsCor.reserve(peaks_cor.size() - 1);
            for (size_t i = 1; i < peaks_cor.size(); ++i) {
                int sampleDelta = peaks_cor[i] - peaks_cor[i - 1];
                peakDiffSamplesCor.push_back(sampleDelta);
                peakDiffMsCor.push_back(sampleDelta * 1000.0 / fs);
            }
            logAnalyze("analyzeSignal: corrected peak sample deltas: %s", vectorToString(peakDiffSamplesCor).c_str());
            logAnalyze("analyzeSignal: corrected peak delta (ms): %s", vectorToString(peakDiffMsCor).c_str());
        }
        // recompute RR list corrected
        m.ibiMs.clear();
        for (size_t i = 1; i < peaks_cor.size(); ++i) m.ibiMs.push_back((peaks_cor[i] - peaks_cor[i - 1]) * 1000.0 / fs);
        m.peakList = peaks_cor;
        if (!m.quality.rejectedIndices.empty()) {
            std::sort(m.quality.rejectedIndices.begin(), m.quality.rejectedIndices.end());
            m.quality.rejectedIndices.erase(std::unique(m.quality.rejectedIndices.begin(), m.quality.rejectedIndices.end()), m.quality.rejectedIndices.end());
        }
    }
	logAnalyze("analyzeSignal: consolidated peaks=%zu (raw=%zu)", m.peakList.size(), m.peakListRaw.size());
	logAnalyze("analyzeSignal: consolidated peaks content: %s", vectorToString(m.peakList).c_str());

	m.rrList = m.ibiMs; // Initially same
	logAnalyze("analyzeSignal: rrList input peaks=%zu", m.peakList.size());
	logAnalyze("analyzeSignal: rr intervals (initial): %s", vectorToString(m.rrList).c_str());

	// Apply HeartPy threshold_rr masking before optional cleaning (parity with HP)
	if (opt.thresholdRR && !m.rrList.empty()) {
		double mean_rr = mean(m.rrList);
		double margin = std::max(0.3 * mean_rr, 300.0);
		double lower = mean_rr - margin;
		double upper = mean_rr + margin;
		std::vector<double> rr_cor;
		rr_cor.reserve(m.rrList.size());
		for (size_t i = 0; i < m.rrList.size(); ++i) {
			double v = m.rrList[i];
			if (!(v <= lower || v >= upper)) rr_cor.push_back(v);
		}
		if (!rr_cor.empty()) {
			m.rrList.swap(rr_cor);
			logAnalyze("analyzeSignal: threshold_rr masked rrList size=%zu", m.rrList.size());
		}
	}

	// Clean RR intervals if requested
	if (opt.cleanRR && !m.rrList.empty()) {
		switch (opt.cleanMethod) {
			case Options::CleanMethod::IQR: {
				double lower, upper;
				m.rrList = removeOutliersIQR(m.rrList, lower, upper);
				break;
			}
			case Options::CleanMethod::Z_SCORE:
				m.rrList = removeOutliersZScore(m.rrList, 3.0);
				break;
			case Options::CleanMethod::QUOTIENT_FILTER:
				m.rrList = removeOutliersQuotientFilter(m.rrList);
				break;
		}
	}
	logAnalyze("analyzeSignal: rrList size=%zu", m.rrList.size());
	logAnalyze("analyzeSignal: rrList content: %s", vectorToString(m.rrList).c_str());

	if (!m.rrList.empty()) {
		double meanIbi = mean(m.rrList);
		m.bpm = 60000.0 / meanIbi;
		logAnalyze("analyzeSignal: calculated BPM=%.2f (rrCount=%zu)", m.bpm, m.rrList.size());
	} else {
		logAnalyze("analyzeSignal: unable to compute BPM (rrCount=0, peaks=%zu)", m.peakList.size());
	}

	// 5) Enhanced Time-domain metrics
	if (!m.rrList.empty()) {
		m.sdnn = std_pop(m.rrList);
		m.mad = calculateMAD(m.rrList);
		
		if (m.rrList.size() >= 2) {
			std::vector<double> diff;
			diff.reserve(m.rrList.size() - 1);
			for (size_t i = 1; i < m.rrList.size(); ++i) {
				diff.push_back(m.rrList[i] - m.rrList[i - 1]);
			}
			
			m.sdsd = std_pop(diff);
			double sumsq = 0.0;
			int over20 = 0;
			int over50 = 0;
			
			for (double d : diff) {
				sumsq += d * d;
				if (std::fabs(d) > 20.0) { ++over20; m.nn20++; }
				if (std::fabs(d) > 50.0) { ++over50; m.nn50++; }
			}
			
			m.rmssd = std::sqrt(sumsq / static_cast<double>(diff.size()));
            // pNN metrics: percent (0-100) or ratio (0..1)
            if (!diff.empty()) {
                // Strict '>' on rounded abs diffs for HeartPy parity
                int over20r = 0, over50r = 0;
                for (double d : diff) {
                    double ad = round6(std::fabs(d));
                    if (ad > 20.0) ++over20r;
                    if (ad > 50.0) ++over50r;
                }
                double r20 = over20r / static_cast<double>(diff.size());
                double r50 = over50r / static_cast<double>(diff.size());
                m.pnn20 = opt.pnnAsPercent ? (100.0 * r20) : r20;
                m.pnn50 = opt.pnnAsPercent ? (100.0 * r50) : r50;
            } else {
                m.pnn20 = 0.0; m.pnn50 = 0.0;
            }
			
			// Enhanced Poincaré analysis
			m.sd1 = m.rmssd / std::sqrt(2.0);
			double sd_diff = sd(diff);
			m.sd2 = std::sqrt(std::max(0.0, 2.0 * m.sdnn * m.sdnn - 0.5 * sd_diff * sd_diff));
			m.sd1sd2Ratio = (m.sd2 > 1e-12) ? m.sd1 / m.sd2 : 0.0;
			m.ellipseArea = PI * m.sd1 * m.sd2;
		}
		
		// Breathing analysis (Hz by default; convert if requested)
		if (m.rrList.size() >= 10) {
			double br_hz = calculateBreathingRate(m.rrList);
			m.breathingRate = opt.breathingAsBpm ? (br_hz * 60.0) : br_hz;
		}
	}

	// RR-based Welch per HeartPy/SciPy (guarded by calcFreq)
	if (opt.calcFreq && m.ibiMs.size() >= 2) {
		// RR_list_cor equivalent
		const std::vector<double>& rr = m.ibiMs;
		// cumulative time in ms
		std::vector<double> rr_x(rr.size());
		double acc = 0.0; for (size_t i=0;i<rr.size();++i){ acc += rr[i]; rr_x[i]=acc; }
		if (rr_x.size() > 1) {
			int resamp_factor = 4;
			int datalen = static_cast<int>((rr_x.size()-1) * resamp_factor);
			if (datalen < 8) datalen = 8;
			double start = rr_x.front();
			double stop = rr_x.back();
			std::vector<double> rr_x_new(datalen);
			for (int i=0;i<datalen;++i) rr_x_new[i] = start + (stop - start) * (static_cast<double>(i) / (datalen - 1));
            // smoothing: prefer Reinsch target SSE if specified, else lambda-based CG, else pre-blend
            std::vector<double> rr_smooth = rr;
            if (opt.rrSplineSTargetSse > 0.0) {
                rr_smooth = smoothRR_TargetSse(rr, opt.rrSplineSTargetSse);
            } else if (opt.rrSplineS > 1e-9) {
                rr_smooth = smoothRR_CG(rr, opt.rrSplineS);
            } else if (opt.rrSplineSmooth > 1e-6) {
                int w = std::max(3, static_cast<int>(std::round((opt.rrSplineSmooth * rr.size()) / 20.0)));
                if (w % 2 == 0) ++w;
                std::vector<double> filt = boxcarSmooth(rr, w);
                for (size_t i = 0; i < rr.size(); ++i) rr_smooth[i] = (1.0 - opt.rrSplineSmooth) * rr[i] + opt.rrSplineSmooth * filt[i];
            }
			// cubic spline interpolate rr_smooth vs rr_x
			CubicSpline sp = buildNaturalCubic(rr_x, rr_smooth);
			std::vector<double> rr_interp(datalen);
			if (sp.ok) {
				for (int i=0;i<datalen;++i) rr_interp[i] = splineEval(sp, rr_x_new[i]);
			} else {
				// fallback linear
				for (int i=0;i<datalen;++i) rr_interp[i] = rr.front();
			}
            // sampling rate per HeartPy
            double dt = mean(rr) / 1000.0; // seconds
            double fs_rr = (dt > 0) ? (1.0 / dt) : 1.0;
            double fs_new = fs_rr * resamp_factor;
            // no explicit detrend in HeartPy calc_fd_measures
			int nperseg = opt.nfft > 0 ? opt.nfft : static_cast<int>(std::round(opt.welchWsizeSec * fs_new));
			if (nperseg <= 0) nperseg = 256;
			if (nperseg > static_cast<int>(rr_interp.size())) nperseg = static_cast<int>(rr_interp.size());
			PSDResult psd = welchPSD(rr_interp, fs_new, nperseg, 0.5);
            if (!psd.freqs.empty()) {
                m.vlf = integrateBand(psd.freqs, psd.psd, 0.0033, 0.04);
                m.lf  = integrateBand(psd.freqs, psd.psd, 0.04,   0.15);
                m.hf  = integrateBand(psd.freqs, psd.psd, 0.15,   0.40);
                m.totalPower = m.vlf + m.lf + m.hf;
                m.lfhf = (m.hf > 1e-12) ? (m.lf / m.hf) : 0.0;
                double sumLFHF = m.lf + m.hf; if (sumLFHF > 1e-12){ m.lfNorm = (m.lf/sumLFHF)*100.0; m.hfNorm = (m.hf/sumLFHF)*100.0; }
                // breathing rate: peak frequency in 0.1–0.4 Hz band (Hz) per HeartPy
                double fpeak=0.0, vmax=-1.0; for (size_t i=0;i<psd.freqs.size();++i){ double f=psd.freqs[i]; if (f>=0.10 && f<=0.40 && psd.psd[i]>vmax){ vmax=psd.psd[i]; fpeak=f; } }
                m.breathingRate = opt.breathingAsBpm ? (fpeak * 60.0) : fpeak;
            } else {
                m.vlf = std::numeric_limits<double>::quiet_NaN();
                m.lf = std::numeric_limits<double>::quiet_NaN();
                m.hf = std::numeric_limits<double>::quiet_NaN();
                m.lfhf = std::numeric_limits<double>::quiet_NaN();
            }
		}
	} else {
		m.vlf = std::numeric_limits<double>::quiet_NaN();
		m.lf = std::numeric_limits<double>::quiet_NaN();
		m.hf = std::numeric_limits<double>::quiet_NaN();
		m.lfhf = std::numeric_limits<double>::quiet_NaN();
	}

	return m;
}

// Outlier detection functions
std::vector<double> removeOutliersIQR(const std::vector<double>& data, double& lowerBound, double& upperBound) {
    if (data.size() < 4) return data;
    
    std::vector<double> sorted = data;
    std::sort(sorted.begin(), sorted.end());
    
    size_t n = sorted.size();
    double q1 = sorted[n / 4];
    double q3 = sorted[3 * n / 4];
    double iqr = q3 - q1;
    
    lowerBound = q1 - 1.5 * iqr;
    upperBound = q3 + 1.5 * iqr;
    
    std::vector<double> result;
    for (double val : data) {
        if (val >= lowerBound && val <= upperBound) {
            result.push_back(val);
        }
    }
    
    return result;
}

std::vector<double> removeOutliersZScore(const std::vector<double>& data, double threshold) {
    if (data.size() < 3) return data;
    
    double meanVal = mean(data);
    double stdVal = sd(data);
    
    if (stdVal < 1e-12) return data;
    
    std::vector<double> result;
    for (double val : data) {
        double zscore = std::abs(val - meanVal) / stdVal;
        if (zscore <= threshold) {
            result.push_back(val);
        }
    }
    
    return result;
}

std::vector<double> removeOutliersQuotientFilter(const std::vector<double>& rrIntervals) {
    if (rrIntervals.size() < 3) return rrIntervals;
    
    std::vector<double> result;
    result.push_back(rrIntervals[0]);
    
    for (size_t i = 1; i < rrIntervals.size() - 1; ++i) {
        double prev = rrIntervals[i-1];
        double curr = rrIntervals[i];
        double next = rrIntervals[i+1];
        
        double q1 = curr / prev;
        double q2 = next / curr;
        
        if (q1 >= 0.8 && q1 <= 1.2 && q2 >= 0.8 && q2 <= 1.2) {
            result.push_back(curr);
        }
    }
    
    if (rrIntervals.size() > 1) {
        result.push_back(rrIntervals.back());
    }
    
    return result;
}

// Quality assessment
QualityInfo assessSignalQuality(const std::vector<double>& signal, const std::vector<int>& peaks, double fs) {
    QualityInfo quality;
    quality.totalBeats = peaks.size();
    
    if (peaks.size() < 2) {
        quality.goodQuality = false;
        quality.qualityWarning = "Insufficient peaks detected";
        return quality;
    }
    
    std::vector<double> rrIntervals;
    for (size_t i = 1; i < peaks.size(); ++i) {
        double rr = (peaks[i] - peaks[i-1]) * 1000.0 / fs;
        rrIntervals.push_back(rr);
    }
    
    int badIntervals = 0;
    for (double rr : rrIntervals) {
        if (rr < 300.0 || rr > 2000.0) {
            badIntervals++;
        }
    }
    
    quality.rejectedBeats = badIntervals;
    quality.rejectionRate = static_cast<double>(badIntervals) / rrIntervals.size();
    quality.goodQuality = quality.rejectionRate < 0.3;
    
    if (!quality.goodQuality) {
        quality.qualityWarning = "High rejection rate";
    }
    
    return quality;
}

bool checkSegmentQuality(const std::vector<int>& rejectedBeats, int totalBeats, double threshold) {
    if (totalBeats == 0) return false;
    double rejectionRate = static_cast<double>(rejectedBeats.size()) / totalBeats;
    return rejectionRate <= threshold;
}

// Breathing analysis
double calculateBreathingRate(const std::vector<double>& rrIntervals, const std::string& method) {
    if (rrIntervals.size() < 10) return 0.0;
    // Build time series from RR intervals (ms) -> seconds
    std::vector<double> t; t.reserve(rrIntervals.size());
    std::vector<double> rrSec; rrSec.reserve(rrIntervals.size());
    double acc = 0.0;
    for (double rr : rrIntervals) {
        double v = rr * 0.001; // seconds
        acc += v;
        t.push_back(acc);
        rrSec.push_back(v);
    }
    // Resample to uniform grid (4 Hz)
    double fs = 4.0;
    double duration = t.back() - t.front();
    int N = std::max(0, static_cast<int>(std::floor(duration * fs)));
    if (N < 16) return 0.0;
    std::vector<double> reg(N);
    double dt = 1.0 / fs;
    for (int i = 0; i < N; ++i) {
        double time = t.front() + i * dt;
        size_t k = 1; while (k < t.size() && t[k] < time) ++k; if (k >= t.size()) k = t.size() - 1;
        double t1 = t[k - 1], t2 = t[k];
        double v1 = rrSec[std::min(k - 1, rrSec.size() - 1)];
        double v2 = rrSec[std::min(k, rrSec.size() - 1)];
        double alpha = (t2 - t1) > 0 ? (time - t1) / (t2 - t1) : 0.0;
        reg[i] = v1 + alpha * (v2 - v1);
    }
    // Detrend
    reg = movingAverageDetrend(reg, static_cast<int>(std::round(2.0 * fs)));
    // Welch PSD
    PSDResult psd = welchPSD(reg, fs, 256, 0.5);
    if (psd.freqs.empty()) return 0.0;
    // Find peak in 0.10-0.40 Hz (HeartPy default breathing band)
    double fpeak = 0.0, pmax = -1.0;
    for (size_t i = 0; i < psd.freqs.size(); ++i) {
        double f = psd.freqs[i];
        if (f >= 0.10 && f <= 0.40 && psd.psd[i] > pmax) {
            pmax = psd.psd[i];
            fpeak = f;
        }
    }
    // Return frequency in Hz; callers convert to BPM if requested via Options
    return (fpeak > 0.0) ? (fpeak) : 0.0;
}

// Utility functions
double calculateMAD(const std::vector<double>& data) {
    if (data.empty()) return 0.0;
    
    std::vector<double> sorted = data;
    std::sort(sorted.begin(), sorted.end());
    double medianVal = sorted[sorted.size() / 2];
    
    std::vector<double> deviations;
    for (double val : data) {
        deviations.push_back(std::abs(val - medianVal));
    }
    
    std::sort(deviations.begin(), deviations.end());
    return deviations[deviations.size() / 2];
}

// Enhanced analysis functions
HeartMetrics analyzeSignalSegmentwise(const std::vector<double>& signal, double fs, const Options& opt) {
    HeartMetrics result;
    
    double segmentLength = opt.segmentWidth * fs;
    double stepSize = segmentLength * (1.0 - opt.segmentOverlap);
    size_t minSegmentSize = static_cast<size_t>(opt.segmentMinSize * fs);
    
    for (size_t start = 0; start < signal.size(); start += static_cast<size_t>(stepSize)) {
        size_t end = std::min(start + static_cast<size_t>(segmentLength), signal.size());
        
        if (end - start < minSegmentSize) break;
        
        std::vector<double> segment(signal.begin() + start, signal.begin() + end);
        
        try {
            HeartMetrics segmentMetrics = analyzeSignal(segment, fs, opt);
            if (segmentMetrics.quality.goodQuality || !opt.rejectSegmentwise) {
                result.segments.push_back(segmentMetrics);
            }
        } catch (const std::exception&) {
            // Skip bad segments
        }
    }
    
    // Compute average metrics across segments
    if (!result.segments.empty()) {
        double avgBpm = 0.0, avgSdnn = 0.0, avgRmssd = 0.0;
        int validSegments = 0;
        
        for (const auto& seg : result.segments) {
            if (seg.bpm > 0) {
                avgBpm += seg.bpm;
                avgSdnn += seg.sdnn;
                avgRmssd += seg.rmssd;
                validSegments++;
            }
        }
        
        if (validSegments > 0) {
            result.bpm = avgBpm / validSegments;
            result.sdnn = avgSdnn / validSegments;
            result.rmssd = avgRmssd / validSegments;
        }
    }
    
    return result;
}

HeartMetrics analyzeRRIntervals(const std::vector<double>& rrMs, const Options& opt) {
    HeartMetrics metrics;
    metrics.rrList = rrMs;

    if (rrMs.empty()) return metrics;

    // Build threshold mask on original RR list (HP: RR_masklist)
    // mask value: 1 = rejected, 0 = accepted (HP semantics)
    std::vector<int> rr_mask(rrMs.size(), 0);
    if (opt.thresholdRR && rrMs.size() >= 1) {
        double mean_rr = mean(rrMs);
        double margin = std::max(0.3 * mean_rr, 300.0);
        double lower = mean_rr - margin;
        double upper = mean_rr + margin;
        for (size_t i = 0; i < rrMs.size(); ++i) {
            if (rrMs[i] <= lower || rrMs[i] >= upper) rr_mask[i] = 1;
        }
        // Build RR_list_cor for time-domain metrics (HP uses RR_list_cor)
        std::vector<double> rr_cor; rr_cor.reserve(rrMs.size());
        for (size_t i = 0; i < rrMs.size(); ++i) if (!rr_mask[i]) rr_cor.push_back(rrMs[i]);
        if (!rr_cor.empty()) metrics.rrList.swap(rr_cor);
    }

    // Apply cleaning if requested (after threshold pass if any)
    if (opt.cleanRR) {
        switch (opt.cleanMethod) {
            case Options::CleanMethod::IQR: {
                double lower, upper;
                metrics.rrList = removeOutliersIQR(metrics.rrList, lower, upper);
                break;
            }
            case Options::CleanMethod::Z_SCORE:
                metrics.rrList = removeOutliersZScore(metrics.rrList, 3.0);
                break;
            case Options::CleanMethod::QUOTIENT_FILTER: {
                // Build quotient filter mask on original RR with existing threshold mask
                std::vector<int> qmask = quotientFilterMask(rrMs, rr_mask, std::max(1, opt.cleanIterations));
                std::vector<double> rr_clean;
                rr_clean.reserve(rrMs.size());
                for (size_t i = 0; i < rrMs.size(); ++i) if (i < qmask.size() && qmask[i] == 0) rr_clean.push_back(rrMs[i]);
                if (!rr_clean.empty()) metrics.rrList.swap(rr_clean);
                rr_mask.swap(qmask);
                break;
            }
        }
    }
    
    if (!metrics.rrList.empty()) {
        double meanRR = mean(metrics.rrList);
        metrics.bpm = 60000.0 / meanRR;
        metrics.sdnn = std_pop(metrics.rrList);
        metrics.mad = calculateMAD(metrics.rrList);
        
        if (metrics.rrList.size() >= 2) {
            // Build masked pair diffs from original RR + combined mask
            std::vector<double> pair_diffs; // signed
            std::vector<double> pair_abs;   // abs
            if (rrMs.size() >= 2) {
                if (rr_mask.empty()) rr_mask.assign(rrMs.size(), 0);
                for (size_t i = 1; i < rrMs.size(); ++i) {
                    if (rr_mask[i] == 0 && rr_mask[i - 1] == 0) {
                        double d = rrMs[i] - rrMs[i - 1];
                        pair_diffs.push_back(d);
                        pair_abs.push_back(std::fabs(d));
                    }
                }
            }

            // SDSD / RMSSD on masked pair diffs
            if (!pair_diffs.empty()) {
                if (opt.sdsdMode == Options::SdsdMode::ABS) metrics.sdsd = std_pop(pair_abs);
                else metrics.sdsd = std_pop(pair_diffs);
                double sumsq = 0.0; for (double d : pair_diffs) sumsq += d * d;
                metrics.rmssd = std::sqrt(sumsq / static_cast<double>(pair_diffs.size()));
                // pNN with strict '>' on rounded abs diffs
                int over20r = 0, over50r = 0;
                for (double ad : pair_abs) {
                    double v = round6(ad);
                    if (v > 20.0) ++over20r;
                    if (v > 50.0) ++over50r;
                }
                metrics.nn20 = over20r;
                metrics.nn50 = over50r;
                double r20 = over20r / static_cast<double>(pair_abs.size());
                double r50 = over50r / static_cast<double>(pair_abs.size());
                metrics.pnn20 = opt.pnnAsPercent ? (100.0 * r20) : r20;
                metrics.pnn50 = opt.pnnAsPercent ? (100.0 * r50) : r50;
            } else {
                metrics.sdsd = 0.0; metrics.rmssd = 0.0; metrics.pnn20 = 0.0; metrics.pnn50 = 0.0; metrics.nn20 = 0; metrics.nn50 = 0;
            }

            // Poincaré (HP masked): use original RR list (rrMs) and threshold mask rr_mask
            // Include adjacent pairs where both are accepted (mask[i]==0 and mask[i+1]==0)
            std::vector<double> x_plus; x_plus.reserve(rrMs.size());
            std::vector<double> x_minus; x_minus.reserve(rrMs.size());
            if (rrMs.size() >= 2) {
                if (rr_mask.empty()) rr_mask.assign(rrMs.size(), 0);
                for (size_t i = 0; i + 1 < rrMs.size(); ++i) {
                    if ((rr_mask[i] + rr_mask[i + 1]) == 0) {
                        x_plus.push_back(rrMs[i]);
                        x_minus.push_back(rrMs[i + 1]);
                    }
                }
            }
            if (x_plus.size() >= 2) {
                // SD1/SD2 via var of rotated axes
                std::vector<double> x_one(x_plus.size());
                std::vector<double> x_two(x_plus.size());
                const double invsqrt2 = 1.0 / std::sqrt(2.0);
                for (size_t i = 0; i < x_plus.size(); ++i) {
                    x_one[i] = (x_plus[i] - x_minus[i]) * invsqrt2;
                    x_two[i] = (x_plus[i] + x_minus[i]) * invsqrt2;
                }
                auto pop_var = [&](const std::vector<double>& v){
                    if (v.empty()) return 0.0; double m = mean(v); double acc=0.0; for(double a: v){ double d=a-m; acc+=d*d; }
                    return acc / static_cast<double>(v.size()); };
                double sd1 = std::sqrt(std::max(0.0, pop_var(x_one)));
                double sd2 = std::sqrt(std::max(0.0, pop_var(x_two)));
                metrics.sd1 = sd1; metrics.sd2 = sd2;
                metrics.sd1sd2Ratio = (metrics.sd2 > 1e-12) ? metrics.sd1 / metrics.sd2 : 0.0;
                metrics.ellipseArea = PI * metrics.sd1 * metrics.sd2;
            } else {
                metrics.sd1 = metrics.rmssd / std::sqrt(2.0);
                metrics.sd2 = std::sqrt(std::max(0.0, 2.0 * metrics.sdnn * metrics.sdnn - 0.5 * metrics.sdsd * metrics.sdsd));
                metrics.sd1sd2Ratio = (metrics.sd2 > 1e-12) ? metrics.sd1 / metrics.sd2 : 0.0;
                metrics.ellipseArea = PI * metrics.sd1 * metrics.sd2;
            }
        }
        
        // Breathing analysis (Hz by default; convert if requested)
        if (metrics.rrList.size() >= 10) {
            double br_hz = calculateBreathingRate(metrics.rrList);
            metrics.breathingRate = opt.breathingAsBpm ? (br_hz * 60.0) : br_hz;
        }
    }
    
    return metrics;
}

// High-precision peak refinement: upsample local windows and re-locate maxima
std::vector<int> interpolatePeaks(const std::vector<double>& signal,
                                  const std::vector<int>& peaks,
                                  double originalFs,
                                  double targetFs) {
    if (peaks.empty() || signal.empty() || targetFs <= originalFs) return peaks;
    std::vector<int> refined;
    refined.reserve(peaks.size());
    int halfWin = static_cast<int>(std::round(0.10 * originalFs)); // 200ms window total (HP interpolate_peaks ~200ms)
    double ratio = targetFs / originalFs;
    for (int p : peaks) {
        int start = std::max(0, p - halfWin);
        int end = std::min(static_cast<int>(signal.size() - 1), p + halfWin);
        int len = end - start + 1;
        if (len <= 2) { refined.push_back(p); continue; }
        // Upsample by linear interpolation
        int upLen = static_cast<int>(std::round(len * ratio));
        if (upLen < 3) { refined.push_back(p); continue; }
        std::vector<double> up(upLen);
        for (int i = 0; i < upLen; ++i) {
            double pos = i / ratio; // position in original samples
            int i0 = static_cast<int>(std::floor(pos));
            double frac = pos - i0;
            int idx0 = start + std::min(i0, len - 2);
            double v0 = signal[idx0];
            double v1 = signal[idx0 + 1];
            up[i] = v0 + frac * (v1 - v0);
        }
        // Locate max in upsampled segment
        int argmax = 0; double vmax = up[0];
        for (int i = 1; i < upLen; ++i) {
            if (up[i] > vmax) { vmax = up[i]; argmax = i; }
        }
        // Parabolic + optional cubic LS refinement around argmax (if neighbors available)
        double refinedUp = static_cast<double>(argmax);
        if (argmax > 0 && argmax + 1 < upLen) {
            double ym1 = up[argmax - 1];
            double y0  = up[argmax];
            double yp1 = up[argmax + 1];
            double denom = (ym1 - 2.0 * y0 + yp1);
            if (std::fabs(denom) > 1e-12) {
                double delta = 0.5 * (ym1 - yp1) / denom; // vertex offset in samples
                refinedUp += delta; // fractional index
            }
        }
        // Cubic LS refinement over 5-point neighborhood if available
        int c = static_cast<int>(std::round(refinedUp));
        if (c - 2 >= 0 && c + 2 < upLen) {
            double x[5] = {-2, -1, 0, 1, 2};
            double y[5];
            for (int i = 0; i < 5; ++i) y[i] = up[c - 2 + i];
            // Build normal equations for cubic y = a x^3 + b x^2 + cx + d
            auto sumPow = [&](int k){ double s=0; for(int i=0;i<5;++i){ double t=1; for(int j=0;j<k;++j) t*=x[i]; s+=t; } return s; };
            double S0=5, S1=sumPow(1), S2=sumPow(2), S3=sumPow(3), S4=sumPow(4), S5=sumPow(5), S6=sumPow(6);
            double A[4][4] = { {S6,S5,S4,S3}, {S5,S4,S3,S2}, {S4,S3,S2,S1}, {S3,S2,S1,S0} };
            double bvec[4] = {0,0,0,0};
            for (int i=0;i<5;++i){ double xi=x[i]; double xi2=xi*xi; double xi3=xi2*xi; double yi=y[i]; bvec[0]+=xi3*yi; bvec[1]+=xi2*yi; bvec[2]+=xi*yi; bvec[3]+=yi; }
            // Solve 4x4 (Gauss-Jordan)
            double M[4][5]; for(int r=0;r<4;++r){ for(int c2=0;c2<4;++c2) M[r][c2]=A[r][c2]; M[r][4]=bvec[r]; }
            bool ok=true; for(int r=0;r<4 && ok;++r){ int piv=r; for(int r2=r+1;r2<4;++r2) if(std::fabs(M[r2][r])>std::fabs(M[piv][r])) piv=r2; if(std::fabs(M[piv][r])<1e-12){ ok=false; break;} if(piv!=r) for(int c2=r;c2<5;++c2) std::swap(M[r][c2],M[piv][c2]); double div=M[r][r]; for(int c2=r;c2<5;++c2) M[r][c2]/=div; for(int rr=0;rr<4;++rr){ if(rr==r) continue; double factor=M[rr][r]; for(int c2=r;c2<5;++c2) M[rr][c2]-=factor*M[r][c2]; } }
            if (ok) {
                double a=M[0][4], bcoef=M[1][4], ccoef=M[2][4], dcoef=M[3][4];
                auto fy=[&](double xx){ return ((a*xx + bcoef)*xx + ccoef)*xx + dcoef; };
                // derivative 3a x^2 + 2b x + c = 0
                double A2=3*a, B2=2*bcoef, C2=ccoef; double bestx=0.0; double besty=fy(0.0);
                if (std::fabs(A2)>1e-12) {
                    double disc=B2*B2 - 4*A2*C2; if (disc>=0){ double r1=(-B2-std::sqrt(disc))/(2*A2); double r2=(-B2+std::sqrt(disc))/(2*A2); for(double xx: {r1,r2}) if(xx>=-2.0 && xx<=2.0){ double val=fy(xx); if(val>besty){ besty=val; bestx=xx; } } }
                } else if (std::fabs(B2)>1e-12) {
                    double xx = -C2 / B2; if (xx>=-2.0 && xx<=2.0){ double val=fy(xx); if(val>besty){ besty=val; bestx=xx; } }
                }
                refinedUp = c + bestx;
            }
        }
        // Map back to original sample index
        double refinedPos = start + (refinedUp / ratio);
        refined.push_back(static_cast<int>(std::round(refinedPos)));
    }
    return refined;
}

// Additional utilities matching header
std::vector<double> calculatePoincare(const std::vector<double>& rrIntervals) {
    std::vector<double> out(4, 0.0);
    if (rrIntervals.size() < 2) return out;
    // SDNN
    double sdnn_val = sd(rrIntervals);
    // successive differences
    std::vector<double> diff;
    diff.reserve(rrIntervals.size() - 1);
    for (size_t i = 1; i < rrIntervals.size(); ++i) diff.push_back(rrIntervals[i] - rrIntervals[i - 1]);
    double rmssd_val = 0.0;
    if (!diff.empty()) {
        double sumsq = 0.0; for (double d : diff) sumsq += d * d; rmssd_val = std::sqrt(sumsq / diff.size());
    }
    double sd1 = rmssd_val / std::sqrt(2.0);
    double sd2 = std::sqrt(std::max(0.0, 2.0 * sdnn_val * sdnn_val - 0.5 * sd(diff) * sd(diff)));
    double ratio = (sd2 > 1e-12) ? sd1 / sd2 : 0.0;
    double area = PI * sd1 * sd2;
    out[0] = sd1; out[1] = sd2; out[2] = ratio; out[3] = area;
    return out;
}

std::pair<std::vector<double>, std::vector<double>> welchPowerSpectrum(
    const std::vector<double>& signal,
    double fs,
    int nfft,
    double overlap) {
    PSDResult psd = welchPSD(signal, fs, nfft, overlap);
    return {psd.freqs, psd.psd};
}

unsigned long long getWelchPsdGuardFallbackCount() { return g_welchGuardFallbackCount.load(); }
unsigned long long getWelchPsdGuardFailureCount() { return g_welchGuardFailureCount.load(); }

void setDeterministic(bool on) { s_deterministic = on; }
bool isDeterministic() { return s_deterministic; }

} // namespace heartpy
