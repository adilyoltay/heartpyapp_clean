#include "heartpy_stream.h"
#include <algorithm>
#include <deque>
#include <cmath>
#include <cassert>
#include <optional>
#include <limits>
#if defined(__ANDROID__)
#include <android/log.h>
#define LOG_TAG "HeartPySNR"
#define LOGD(fmt, ...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, fmt, ##__VA_ARGS__)
#elif defined(__APPLE__)
#include <cstdio>
#define LOGD(fmt, ...)                                                                                             \
    do {                                                                                                           \
        std::fprintf(stderr, "[HeartPySNR] " fmt "\n", ##__VA_ARGS__);                                           \
        std::fflush(stderr);                                                                                       \
    } while (0)
#else
#include <cstdio>
#define LOGD(fmt, ...)                                                                                             \
    do {                                                                                                           \
        std::fprintf(stderr, "[HeartPySNR] " fmt "\n", ##__VA_ARGS__);                                           \
        std::fflush(stderr);                                                                                       \
    } while (0)
#endif

namespace heartpy {

namespace { constexpr double kSnrFallbackDb = -5.0; }

// Defensive helpers
static inline int clampIndexInt(int i, int n) {
    if (i < 0) return 0; if (i >= n) return (n > 0 ? n - 1 : 0); return i;
}
static inline bool inRangeIdx(int i, int n) { return (i >= 0) && (i < n); }
static inline bool absToRel(size_t abs, size_t firstAbs, size_t n, size_t& rel) {
    if (abs < firstAbs) return false; size_t v = abs - firstAbs; if (v >= n) return false; rel = v; return true;
}

// Size safety helpers
static inline size_t safeSizeMul(double a, double b, size_t cap) {
    if (!(std::isfinite(a) && std::isfinite(b))) return 0;
    if (a <= 0.0 || b <= 0.0) return 0;
    long double prod = (long double)a * (long double)b;
    if (prod < 0.0L) prod = 0.0L;
    long double capld = static_cast<long double>(cap);
    if (prod > capld) return cap;
    size_t v = static_cast<size_t>(prod);
    return (v > cap) ? cap : v;
}
static constexpr double MAX_WINDOW_SEC = 300.0; // acceptance memory limit

#ifdef HEARTPY_LOCK_TIMING
static std::vector<double> g_lock1_times_us; // snapshot lock
static std::vector<double> g_lock2_times_us; // commit lock
static inline void push_lock_time(int which, double us) {
    (which == 2 ? g_lock2_times_us : g_lock1_times_us).push_back(us);
}
void RealtimeAnalyzer::lockStatsGet(int which, double& avg_us, double& p95_us, bool reset) {
    avg_us = 0.0; p95_us = 0.0;
    auto& v = (which == 2 ? g_lock2_times_us : g_lock1_times_us);
    if (!v.empty()) {
        double sum = 0.0; for (double x : v) sum += x; avg_us = sum / v.size();
        auto tmp = v; std::sort(tmp.begin(), tmp.end());
        size_t idx = (size_t)std::min(tmp.size() - 1, (size_t)std::floor(0.95 * (tmp.size() - 1)));
        p95_us = tmp[idx];
        if (reset) v.clear();
    }
}
void RealtimeAnalyzer::recordLockHold(int which, double us) { push_lock_time(which, us); }
#endif
// Local HP-style helpers (mirrors core behavior, kept local to avoid linkage deps)
static std::vector<double> rollingMeanHP_local(const std::vector<double>& data, double fs, double windowSeconds) {
    const int N = static_cast<int>(windowSeconds * fs);
    const int n = static_cast<int>(data.size());
    if (N <= 1 || n == 0 || N > n) {
        double ssum = 0.0; for (double v : data) ssum += v; double m = (n > 0 ? (ssum / n) : 0.0);
        return std::vector<double>(n, m);
    }
    std::vector<double> rol; rol.reserve(n - N + 1);
    double s = 0.0; for (int i = 0; i < N; ++i) s += data[i];
    rol.push_back(s / N);
    for (int i = N; i < n; ++i) { s += data[i]; s -= data[i - N]; rol.push_back(s / N); }
    int n_miss = static_cast<int>(std::abs(n - static_cast<int>(rol.size())) / 2);
    std::vector<double> out; out.reserve(n);
    for (int i = 0; i < n_miss; ++i) out.push_back(rol.front());
    out.insert(out.end(), rol.begin(), rol.end());
    while ((int)out.size() < n) out.push_back(rol.back());
    if ((int)out.size() > n) out.resize(n);
    return out;
}

static std::vector<int> detectPeaksHP_local(const std::vector<double>& x, const std::vector<double>& rol_mean, double ma_perc, double fs) {
    const int n = static_cast<int>(x.size());
    if (n == 0 || (int)rol_mean.size() != n) return {};
    double ssum = 0.0; for (double v : rol_mean) ssum += v; double mn = ((rol_mean.empty() ? 0.0 : (ssum / (double)rol_mean.size())) / 100.0) * ma_perc;
    std::vector<double> thr(n);
    for (int i = 0; i < n; ++i) thr[i] = rol_mean[i] + mn;
    std::vector<int> maskIdx; maskIdx.reserve(n);
    for (int i = 0; i < n; ++i) if (x[i] > thr[i]) maskIdx.push_back(i);
    if (maskIdx.empty()) return {};
    std::vector<int> edges; edges.push_back(0);
    for (size_t i = 1; i < maskIdx.size(); ++i) if (maskIdx[i] - maskIdx[i-1] > 1) edges.push_back((int)i);
    edges.push_back((int)maskIdx.size());
    std::vector<int> peaklist; peaklist.reserve(edges.size());
    for (size_t e = 0; e + 1 < edges.size(); ++e) {
        int a = edges[e], b = edges[e+1]; if (a >= b) continue;
        int best_idx = maskIdx[a]; double best_val = x[best_idx];
        for (int j = a + 1; j < b; ++j) { int idx = maskIdx[j]; if (x[idx] > best_val) { best_val = x[idx]; best_idx = idx; } }
        peaklist.push_back(best_idx);
    }
    if (!peaklist.empty()) {
        if (peaklist[0] <= (int)((fs / 1000.0) * 150.0)) peaklist.erase(peaklist.begin());
    }
    return peaklist;
}

// Collapse peaks closer than refractory to the strongest amplitude
static std::vector<int> consolidateByRefractory(const std::vector<int>& peaks,
                                                const std::vector<double>& x,
                                                int refractorySamples) {
    if (peaks.empty()) return {};
    std::vector<int> out;
    int current = peaks[0];
    double currentVal = x[current];
    for (size_t i = 1; i < peaks.size(); ++i) {
        int p = peaks[i];
        if (p - current <= refractorySamples) {
            // within refractory window: keep the stronger
            if (x[p] > currentVal) { current = p; currentVal = x[p]; }
        } else {
            out.push_back(current);
            current = p;
            currentVal = x[p];
        }
    }
    out.push_back(current);
    return out;
}

static std::vector<SBiquad> designBandpassStream(double fs, double lowHz, double highHz, int sections) {
    std::vector<SBiquad> chain;
    if (lowHz <= 0.0 && highHz <= 0.0) return chain;
    if (fs <= 0.0) return chain;
    sections = std::max(1, sections);
    double f0 = (lowHz > 0.0 && highHz > 0.0) ? 0.5 * (lowHz + highHz)
                                              : std::max(0.001, (lowHz > 0.0 ? lowHz : highHz));
    double bw = (lowHz > 0.0 && highHz > 0.0) ? (highHz - lowHz) : std::max(0.25, f0 * 0.5);
    double Q = std::max(0.2, f0 / std::max(1e-9, bw));
    const double w0 = 2.0 * 3.141592653589793 * f0 / fs;
    const double alpha = std::sin(w0) / (2.0 * Q);
    const double cosw0 = std::cos(w0);
    double b0 =   alpha;
    double b1 =   0.0;
    double b2 =  -alpha;
    double a0 =   1.0 + alpha;
    double a1 =  -2.0 * cosw0;
    double a2 =   1.0 - alpha;
    SBiquad bi;
    bi.b0 = b0 / a0;
    bi.b1 = b1 / a0;
    bi.b2 = b2 / a0;
    bi.a1 = a1 / a0;
    bi.a2 = a2 / a0;
    for (int i = 0; i < sections; ++i) chain.push_back(bi);
    return chain;
}

static std::vector<SBiquadD> designBandpassStreamD(double fs, double lowHz, double highHz, int sections) {
    std::vector<SBiquadD> chain;
    if (lowHz <= 0.0 && highHz <= 0.0) return chain;
    if (fs <= 0.0) return chain;
    sections = std::max(1, sections);
    double f0 = (lowHz > 0.0 && highHz > 0.0) ? 0.5 * (lowHz + highHz)
                                              : std::max(0.001, (lowHz > 0.0 ? lowHz : highHz));
    double bw = (lowHz > 0.0 && highHz > 0.0) ? (highHz - lowHz) : std::max(0.25, f0 * 0.5);
    double Q = std::max(0.2, f0 / std::max(1e-9, bw));
    const double w0 = 2.0 * 3.141592653589793 * f0 / fs;
    const double alpha = std::sin(w0) / (2.0 * Q);
    const double cosw0 = std::cos(w0);
    double b0 =   alpha;
    double b1 =   0.0;
    double b2 =  -alpha;
    double a0 =   1.0 + alpha;
    double a1 =  -2.0 * cosw0;
    double a2 =   1.0 - alpha;
    SBiquadD bi;
    bi.b0 = b0 / a0;
    bi.b1 = b1 / a0;
    bi.b2 = b2 / a0;
    bi.a1 = a1 / a0;
    bi.a2 = a2 / a0;
    for (int i = 0; i < sections; ++i) chain.push_back(bi);
    return chain;
}

// helpers (local)
static inline double meanVec(const std::vector<double>& v) {
    if (v.empty()) return 0.0; double s = 0.0; for (double x : v) s += x; return s / static_cast<double>(v.size());
}
static inline double std_pop_vec(const std::vector<double>& v) {
    if (v.empty()) return 0.0; double m = meanVec(v); double acc = 0.0; for (double x : v) { double d = x - m; acc += d * d; } return acc / static_cast<double>(v.size());
}
static inline double round6_local(double x) { return std::round(x * 1e6) / 1e6; }

RealtimeAnalyzer::RealtimeAnalyzer(double fs, const Options& opt)
    : fs_(fs), opt_(opt) {
    if (fs_ <= 0.0) fs_ = 50.0;
    if (windowSec_ < 1.0) windowSec_ = 10.0;
    if (windowSec_ > MAX_WINDOW_SEC) windowSec_ = MAX_WINDOW_SEC;
    if (updateSec_ <= 0.0) updateSec_ = 1.0;
    updateSec_ = std::clamp(windowSec_ * 0.08, 0.2, 0.5);
    // Reserve with safe size arithmetic
    size_t margin = 8 * static_cast<size_t>(std::ceil(fs_));
    size_t cap = safeSizeMul(windowSec_, fs_, SIZE_MAX / 4);
    cap = (cap > SIZE_MAX - margin) ? (SIZE_MAX - margin) : (cap + margin);
    m_signal_buffer.reserve(cap);
    filt_.reserve(cap);
    effectiveFs_ = fs_;
    firstTsApprox_ = 0.0;
    lastTs_ = 0.0;
    warmupStartTs_ = std::numeric_limits<double>::quiet_NaN();
    // Streaming filter design
    if (opt_.lowHz > 0.0 || opt_.highHz > 0.0) {
        bool useD = opt_.highPrecision || opt_.deterministic;
        if (useD) bqD_ = designBandpassStreamD(fs_, opt_.lowHz, opt_.highHz, std::max(1, opt_.iirOrder));
        else bq_ = designBandpassStream(fs_, opt_.lowHz, opt_.highHz, std::max(1, opt_.iirOrder));
    }
    // Rolling stats window ~0.75s
    winSamples_ = std::max(5, static_cast<int>(std::lround(0.75 * fs_)));
    refractorySamples_ = std::max(1, static_cast<int>(std::lround((opt_.refractoryMs * 0.001) * fs_)));
    firstAbs_ = 0;
    totalAbs_ = 0;
    rollSum_ = 0.0;
    rollSumSq_ = 0.0;
    // HP thresholding state
    maPerc_ = std::max(10.0, std::min(60.0, opt_.maPerc));
    hpThreshold_ = opt_.useHPThreshold;
    if (opt_.snrTauSec > 0.0) {
        snrTauSec_ = std::max(0.1, opt_.snrTauSec);
    }
    if (opt_.snrActiveTauSec <= 0.0) {
        opt_.snrActiveTauSec = std::max(snrTauSec_, 0.1);
    }
}

void RealtimeAnalyzer::setWindowSeconds(double sec) {
    std::lock_guard<std::mutex> lock(dataMutex_);
    double clamped = std::max(1.0, std::min(MAX_WINDOW_SEC, sec));
    if (clamped != windowSec_) {
    windowSec_ = clamped;
        // Restart warm-up timing so confidence re-gates after substantive window changes
        if ((useRing_ && ringSignal_.size() > 0) || (!useRing_ && !m_signal_buffer.empty())) {
            warmupStartTs_ = lastTs_;
        } else {
            warmupStartTs_ = std::numeric_limits<double>::quiet_NaN();
        }
    } else {
        windowSec_ = clamped;
    }
    updateSec_ = std::clamp(windowSec_ * 0.08, 0.2, 0.5);
    trimToWindow();
}

void RealtimeAnalyzer::setUpdateIntervalSeconds(double sec) {
    std::lock_guard<std::mutex> lock(dataMutex_);
    updateSec_ = std::max(0.1, sec);
    ++paramChangeEventsTotal_;
}

void RealtimeAnalyzer::append(const float* x, size_t n) {
    if (!x || n == 0) return;
    // Append and process new samples incrementally
    const size_t prevLen = m_signal_buffer.size();
    m_signal_buffer.insert(m_signal_buffer.end(), x, x + n);
    const size_t newLen = m_signal_buffer.size();
    if (filt_.size() < prevLen) filt_.resize(prevLen);
    if (m_signal_buffer.size() > filt_.size()) filt_.resize(m_signal_buffer.size());
    // timebase (nominal fs)
    if (prevLen == 0) {
        firstTsApprox_ = 0.0;
        lastTs_ = static_cast<double>(n) / fs_;
        if (!std::isfinite(warmupStartTs_)) warmupStartTs_ = 0.0;
    } else {
        lastTs_ += static_cast<double>(n) / fs_;
    }
    // Process new portion
    for (size_t i = prevLen; i < newLen; ++i) {
        float s = m_signal_buffer[i];
        bool useD = opt_.highPrecision || opt_.deterministic;
        float yout;
        if (useD && !bqD_.empty()) {
            double yd = static_cast<double>(s);
            for (auto &bi : bqD_) yd = bi.process(yd);
            yout = static_cast<float>(yd);
        } else {
            float y = s;
            for (auto &bi : bq_) y = bi.process(y);
            yout = y;
        }
        filt_[i] = yout;
        // rolling window update
        rollWin_.push_back(yout);
        rollSum_ += yout;
        rollSumSq_ += static_cast<double>(yout) * static_cast<double>(yout);
        // rectified update for thresholding
        {
            float yr = std::max(0.0f, yout);
            rollWinRect_.push_back(yr);
            rollRectSum_ += yr;
            rollRectSumSq_ += static_cast<double>(yr) * static_cast<double>(yr);
            while (!rectMinQ_.empty() && rectMinQ_.back() > yr) rectMinQ_.pop_back();
            rectMinQ_.push_back(yr);
            while (!rectMaxQ_.empty() && rectMaxQ_.back() < yr) rectMaxQ_.pop_back();
            rectMaxQ_.push_back(yr);
        }
        while ((int)rollWin_.size() > winSamples_) {
            float u = rollWin_.front(); rollWin_.pop_front();
            rollSum_ -= u; rollSumSq_ -= static_cast<double>(u) * static_cast<double>(u);
        }
        while ((int)rollWinRect_.size() > winSamples_) {
            float u = rollWinRect_.front(); rollWinRect_.pop_front();
            rollRectSum_ -= u; rollRectSumSq_ -= static_cast<double>(u) * static_cast<double>(u);
            if (!rectMinQ_.empty() && rectMinQ_.front() == u) rectMinQ_.pop_front();
            if (!rectMaxQ_.empty() && rectMaxQ_.front() == u) rectMaxQ_.pop_front();
        }
        // incremental local-max detection using 1-sample look-ahead
        size_t k = i;
        if (k >= 2) {
            float y2 = filt_[k - 2];
            float y1 = filt_[k - 1];
            float y0 = filt_[k - 0];
            if (y1 > y2 && y1 >= y0) {
                int nwin = static_cast<int>(rollWin_.size());
                double mean = (nwin > 0 ? (rollSum_ / nwin) : 0.0);
                double var = (nwin > 0 ? (rollSumSq_ / nwin - mean * mean) : 0.0);
                if (var < 0.0) var = 0.0; double sd = std::sqrt(var);
                double thr;
                double y1Cmp = y1;
                if (hpThreshold_) {
                    // Positive-baseline scaling over the rolling window [0..1024]
                    double vmin = y1, vmax = y1;
                    for (float vv : rollWin_) { if (vv < vmin) vmin = vv; if (vv > vmax) vmax = vv; }
                    double den = std::max(1e-6, vmax - vmin);
                    double scaledMean = (mean - vmin) / den * 1024.0;
                    // temporary lift boost window (if applicable)
                    const double effFsLocThr = (effectiveFs_ > 1e-6 ? effectiveFs_ : fs_);
                    size_t testAbs = firstAbs_ + (k - 1);
                    double tnowThr = firstTsApprox_ + ((double)(testAbs - firstAbs_)) / effFsLocThr;
                    double lift = baseLift_ + ((tnowThr < tempLiftUntil_) ? tempLiftBoost_ : 0.0);
                    thr = scaledMean + lift;
                    y1Cmp = (y1 - vmin) / den * 1024.0;
                } else {
                    thr = mean + (opt_.thresholdScale * sd);
                }
                // absolute sample index of y1
                size_t absIdx = firstAbs_ + (k - 1);
                if (y1Cmp > thr) {
                    // RR-predicted gating
                    const double effFsLoc = (effectiveFs_ > 1e-6 ? effectiveFs_ : fs_);
                    bool allowPeak = true;
                    if (!peaksAbs_.empty()) {
                        size_t lastAbs = peaksAbs_.back();
                        double rr_new_ms = (double)(absIdx - lastAbs) / effFsLoc * 1000.0;
                        double tnow = firstTsApprox_ + ((double)(absIdx - firstAbs_)) / effFsLoc;
                        double bpm_prior = bpmEmaValid_ ? bpmEma_ : (0.5 * (opt_.bpmMin + opt_.bpmMax));
                        bpm_prior = std::max(opt_.bpmMin, std::min(opt_.bpmMax, bpm_prior));
                        double rr_prior_ms = std::max(opt_.minRRFloorRelaxed, std::min(opt_.minRRCeiling, 60000.0 / std::max(1e-6, bpm_prior)));
                        int acceptedRR = std::max(0, (int)acceptedPeaksTotal_ - 1);
                        bool gateRel = (tnow >= 15.0) && (acceptedRR >= 10) && (bpmEmaValid_ && bpmEma_ < 100.0);
                        double floor_ms = gateRel ? opt_.minRRFloorRelaxed : opt_.minRRFloorStrict;
                        double min_rr_ms = std::max(0.7 * rr_prior_ms, floor_ms);
                        // Unified long-RR gating when soft/hard/hint is active
                        if (softDoublingActive_ || doublingActive_ || doublingHintActive_) {
                            double longEst = 0.0;
                            if (doublingLongRRms_ > 0.0) longEst = std::max(longEst, doublingLongRRms_);
                            if (!lastRR_.empty()) {
                                double med = medianOfRR(lastRR_);
                                longEst = std::max(longEst, 2.0 * med);
                            }
                            if (lastF0Hz_ > 1e-9) longEst = std::max(longEst, 1000.0 / lastF0Hz_);
                            if (longEst > 0.0) {
                                longEst = std::clamp(longEst, 600.0, opt_.minRRCeiling);
                                double minSoft = std::clamp(opt_.minRRGateFactor * longEst, opt_.minRRFloorRelaxed, opt_.minRRCeiling);
                                min_rr_ms = std::max(min_rr_ms, minSoft);
                                // Hard doubling fallback bounds folded here for coherence
                                if (doublingActive_ && (doublingLongRRms_ > 0.0)) {
                                    if (tnow <= hardFallbackUntil_) {
                                        min_rr_ms = std::max(min_rr_ms, 0.9 * doublingLongRRms_);
                                    } else if (tnow < doublingHoldUntil_) {
                                        min_rr_ms = std::max(min_rr_ms, 0.8 * doublingLongRRms_);
                                    }
                                }
                            }
                        }
                        if (rr_new_ms < min_rr_ms) {
                            // strongest exception
                            size_t relLast = lastAbs >= firstAbs_ ? (lastAbs - firstAbs_) : 0;
                            float lastVal = (relLast < filt_.size() ? filt_[relLast] : y1);
                            double lastCmp = lastVal;
                            if (hpThreshold_) {
                                double vmin2 = y1, vmax2 = y1; for (float vv : rollWin_) { if (vv < vmin2) vmin2 = vv; if (vv > vmax2) vmax2 = vv; }
                                double den2 = std::max(1e-6, vmax2 - vmin2);
                                lastCmp = (lastVal - vmin2) / den2 * 1024.0;
                            }
                            if (!(y1Cmp > lastCmp + 1.0 * sd)) allowPeak = false;
                        }
                        // Rejection tracking and temporary lift/refractory bias
                        
                        if (!allowPeak) {
                            if ((tnow - shortRejectWindowStart_) > 3.0) { shortRejectWindowStart_ = tnow; shortRejectCount_ = 0; }
                            ++shortRejectCount_;
                            if (shortRejectCount_ > 3) {
                                tempLiftBoost_ = std::max(tempLiftBoost_, 10.0);
                                tempLiftUntil_ = tnow + 2.0;
                                int capExtra = (int)std::lround(std::max(0.0, 0.35 - (opt_.refractoryMs * 0.001)) * effFsLoc);
                                dynRefExtraSamples_ = std::min(std::max(dynRefExtraSamples_, (int)std::lround(0.05 * effFsLoc)), capExtra);
                                dynRefUntil_ = tnow + 2.0;
                            }
                        }
                        if (tnow > dynRefUntil_) dynRefExtraSamples_ = 0;
                        // Diagnostics: track applied refractory and min-RR bound in this path
                        int dynBaseRef = (int)std::lround(std::clamp(0.4 * rr_prior_ms, 280.0, 450.0) * 0.001 * effFsLoc);
                        int appliedRef = dynBaseRef + dynRefExtraSamples_;
                        double tcur = tnow;
                        if (doublingActive_ && (tcur <= hardFallbackUntil_)) {
                            int fallbackRef = (int)std::lround(std::min(450.0, 0.5 * rr_prior_ms) * 0.001 * effFsLoc);
                            appliedRef = std::max(appliedRef, fallbackRef);
                        }
                        lastRefMsActive_ = appliedRef * 1000.0 / effFsLoc;
                        lastMinRRBoundMs_ = min_rr_ms;
                    }
                    if (allowPeak) {
                        if (peaksAbs_.empty()) {
                            peaksAbs_.push_back(absIdx);
                            lastAcceptedAmpCmp_ = y1Cmp;
                            ++acceptedPeaksTotal_;
                        } else {
                            size_t lastAbs = peaksAbs_.back();
                            // dynamic base refractory + temporary extras, with hard fallback boost
                            double bpm_prior2 = bpmEmaValid_ ? bpmEma_ : (0.5 * (opt_.bpmMin + opt_.bpmMax));
                            double rr_prior_ms2 = std::max(400.0, std::min(1200.0, 60000.0 / std::max(1e-6, bpm_prior2)));
                            int baseRef2 = (int)std::lround(std::clamp(0.4 * rr_prior_ms2, 280.0, 450.0) * 0.001 * effFsLoc);
                            int refractoryNow = std::max(1, baseRef2) + dynRefExtraSamples_;
                            double tcur2 = firstTsApprox_ + ((double)(absIdx - firstAbs_)) / effFsLoc;
                            if (doublingActive_ && (tcur2 <= hardFallbackUntil_)) {
                                int fallbackRef = (int)std::lround(std::min(450.0, 0.5 * rr_prior_ms2) * 0.001 * effFsLoc);
                                refractoryNow = std::max(refractoryNow, fallbackRef);
                            }
                            if ((absIdx - lastAbs) >= (size_t)std::max(1, refractoryNow)) {
                                peaksAbs_.push_back(absIdx);
                                lastAcceptedAmpCmp_ = y1Cmp;
                                ++acceptedPeaksTotal_;
                            } else {
                                // strongest-within-refractory: replace if stronger
                                size_t relLast = lastAbs >= firstAbs_ ? (lastAbs - firstAbs_) : 0;
                                float lastVal = (relLast < filt_.size() ? filt_[relLast] : y1);
                                double lastCmp = lastVal;
                                if (hpThreshold_) {
                                    double vmin = y1, vmax = y1; for (float vv : rollWin_) { if (vv < vmin) vmin = vv; if (vv > vmax) vmax = vv; }
                                    double den2 = std::max(1e-6, vmax - vmin);
                                    lastCmp = (lastVal - vmin) / den2 * 1024.0;
                                }
                                if (y1Cmp > lastCmp) peaksAbs_.back() = absIdx;
                            }
                        }
                    }
                }
            }
        }
        ++totalAbs_;
    }
    // Rebuild downsampled display buffer (simple decimation)
    const double effFs = (effectiveFs_ > 1e-6 ? effectiveFs_ : fs_);
    int stride = std::max(1, (int)std::lround(effFs / std::max(10.0, displayHz_)));
    displayBuf_.clear();
    if (useRing_) {
        std::vector<float> tmp; ringFilt_.snapshot(tmp);
        displayBuf_.reserve(tmp.size() / stride + 1);
        for (size_t idx = 0; idx < tmp.size(); idx += (size_t)stride) displayBuf_.push_back(tmp[idx]);
    } else {
        displayBuf_.reserve(filt_.size() / stride + 1);
        for (size_t idx = 0; idx < filt_.size(); idx += (size_t)stride) displayBuf_.push_back(filt_[idx]);
    }
    trimToWindow();
}

void RealtimeAnalyzer::trimToWindow() {
    const double effFs = (effectiveFs_ > 1e-6 ? effectiveFs_ : fs_);
    const size_t maxSamples = safeSizeMul(std::min(windowSec_, MAX_WINDOW_SEC), effFs, SIZE_MAX / 4);
    if (useRing_) {
        size_t cur = ringFilt_.size();
        firstAbs_ = (totalAbs_ > cur) ? (totalAbs_ - cur) : 0;
        firstTsApprox_ = lastTs_ - static_cast<double>(cur) / effFs;
        while (!peaksAbs_.empty() && peaksAbs_.front() < firstAbs_) peaksAbs_.erase(peaksAbs_.begin());
        lastPeaks_.clear(); lastRR_.clear();
        for (size_t j = 0; j < peaksAbs_.size(); ++j) {
            size_t rel = peaksAbs_[j] - firstAbs_;
            lastPeaks_.push_back(static_cast<int>(rel));
            if (j > 0) {
                double dt = static_cast<double>(peaksAbs_[j] - peaksAbs_[j - 1]) / effFs;
                lastRR_.push_back(dt * 1000.0);
            }
        }
    } else if (m_signal_buffer.size() > maxSamples) {
        const size_t drop = m_signal_buffer.size() - maxSamples;
        m_signal_buffer.erase(m_signal_buffer.begin(), m_signal_buffer.begin() + drop);
        if (!m_timestamps.empty()) {
            if (m_timestamps.size() >= drop) {
                m_timestamps.erase(m_timestamps.begin(), m_timestamps.begin() + drop);
            } else {
                m_timestamps.clear();
            }
        }
        if (filt_.size() >= drop) filt_.erase(filt_.begin(), filt_.begin() + drop);
        droppedSamplesLast_ += drop; droppedSamplesTotal_ += drop; ++dropConsecPolls_;
        // Approximate firstTs by backing off from lastTs
        firstTsApprox_ = lastTs_ - static_cast<double>(m_signal_buffer.size()) / effFs;
        firstAbs_ += drop;
        // prune peaks outside window; rebuild RR/peaks relative indices
        while (!peaksAbs_.empty() && peaksAbs_.front() < firstAbs_) peaksAbs_.erase(peaksAbs_.begin());
        lastPeaks_.clear(); lastRR_.clear();
        for (size_t j = 0; j < peaksAbs_.size(); ++j) {
            size_t rel = peaksAbs_[j] - firstAbs_;
            lastPeaks_.push_back(static_cast<int>(rel));
            if (j > 0) {
                double dt = static_cast<double>(peaksAbs_[j] - peaksAbs_[j - 1]) / effFs;
                lastRR_.push_back(dt * 1000.0);
            }
        }
    } else { dropConsecPolls_ = 0; }
    // Trim display buffer to the same time window length in seconds
    const size_t maxDisp = safeSizeMul(std::min(windowSec_, MAX_WINDOW_SEC), std::max(10.0, displayHz_), SIZE_MAX / 8);
    if (displayBuf_.size() > maxDisp) {
        const size_t drop = displayBuf_.size() - maxDisp;
        displayBuf_.erase(displayBuf_.begin(), displayBuf_.begin() + drop);
    }
}

void RealtimeAnalyzer::push(const float* samples, size_t n, double /*t0*/) {
    if (!samples || n == 0) return;
    size_t maxBatch = (size_t)std::ceil(std::max(1.0, 10.0) * fs_);
    if (n > maxBatch) {
        n = maxBatch; // clamp oversized batch
        ++clampedBatchesTotal_;
        // optional: debug log (non-fatal)
        // fprintf(stderr, "[heartpy] push(): batch clamped to %zu samples\n", n);
    }
    std::lock_guard<std::mutex> lock(dataMutex_);
    append(samples, n);
}

void RealtimeAnalyzer::push(const std::vector<double>& samples, double /*t0*/) {
    if (samples.empty()) return;
    size_t n = samples.size();
    size_t maxBatch = (size_t)std::ceil(std::max(1.0, 10.0) * fs_);
    if (n > maxBatch) { n = maxBatch; ++clampedBatchesTotal_; } // clamp
    std::vector<float> tmp(n);
    for (size_t i = 0; i < n; ++i) tmp[i] = static_cast<float>(samples[i]);
    std::lock_guard<std::mutex> lock(dataMutex_);
    append(tmp.data(), tmp.size());
}

void RealtimeAnalyzer::push(const float* samples, const double* timestamps, size_t n) {
    if (!samples || !timestamps || n == 0) return;
    size_t maxBatch = (size_t)std::ceil(std::max(1.0, 10.0) * fs_);
    if (n > maxBatch) { n = maxBatch; ++clampedBatchesTotal_; } // clamp
    std::lock_guard<std::mutex> lock(dataMutex_);
    // Update effective Fs using timestamps
    double t0 = timestamps[0];
    double t1 = timestamps[n - 1];
    if (n >= 2) {
        double dt = (t1 - t0) / static_cast<double>(n - 1);
        if (dt > 1e-6) {
            double fsBatch = 1.0 / dt;
            if (effectiveFs_ <= 0.0) effectiveFs_ = fsBatch;
            else effectiveFs_ = (1.0 - emaAlpha_) * effectiveFs_ + emaAlpha_ * fsBatch;
        }
    }
    if (useRing_) {
        if (ringFilt_.size() == 0) {
            firstTsApprox_ = t0;
            if (!std::isfinite(warmupStartTs_)) warmupStartTs_ = t0;
        }
        double lastSeenTs = lastTs_;
        for (size_t i = 0; i < n; ++i) {
            double ts = timestamps[i];
            if (ts < lastSeenTs) { ++timestampBacktrackEventsTotal_;
                ++timestampsSkippedTotal_; continue; }
            if ((ts - lastSeenTs) > 2.0) { ++timeJumpEventsTotal_; }
            float s = samples[i];
            bool useD = opt_.highPrecision || opt_.deterministic;
            if (useD && !bqD_.empty()) {
                double yd = static_cast<double>(s);
                for (auto &bi : bqD_) yd = bi.process(yd);
                ringSignal_.push_back(s);
                ringFilt_.push_back(static_cast<float>(yd));
            } else {
                float y = s;
                for (auto &bi : bq_) y = bi.process(y);
                ringSignal_.push_back(s);
                ringFilt_.push_back(y);
            }
            // track timestamp alongside ring contents (trimmed below)
            m_timestamps.push_back(ts);
            ++totalAbs_;
            lastSeenTs = ts;
        }
        lastTs_ = lastSeenTs;
        firstAbs_ = (totalAbs_ > ringFilt_.size()) ? (totalAbs_ - ringFilt_.size()) : 0;
        // Ensure timestamps mirror the current ring window length
        size_t curWin = ringFilt_.size();
        if (m_timestamps.size() > curWin) {
            size_t dropTs = m_timestamps.size() - curWin;
            m_timestamps.erase(m_timestamps.begin(), m_timestamps.begin() + dropTs);
        }
        return;
    }
    if (m_signal_buffer.empty()) {
        firstTsApprox_ = t0;
        if (!std::isfinite(warmupStartTs_)) warmupStartTs_ = t0;
    }
    lastTs_ = t1;
    // Process each incoming sample through the same path as append()
    const size_t prevLen = m_signal_buffer.size();
    m_signal_buffer.insert(m_signal_buffer.end(), samples, samples + n);
    // mirror timestamps for non-ring window
    m_timestamps.insert(m_timestamps.end(), timestamps, timestamps + n);
    if (filt_.size() < prevLen) filt_.resize(prevLen);
    if (m_signal_buffer.size() > filt_.size()) filt_.resize(m_signal_buffer.size());
    for (size_t i = 0; i < n; ++i) {
        size_t dst = prevLen + i;
        float s = samples[i];
        bool useD = opt_.highPrecision || opt_.deterministic;
        float yout;
        if (useD && !bqD_.empty()) {
            double yd = static_cast<double>(s);
            for (auto &bi : bqD_) yd = bi.process(yd);
            yout = static_cast<float>(yd);
        } else {
            float y = s;
            for (auto &bi : bq_) y = bi.process(y);
            yout = y;
        }
        filt_[dst] = yout;
        // rolling window update
        rollWin_.push_back(yout);
        rollSum_ += yout;
        rollSumSq_ += static_cast<double>(yout) * static_cast<double>(yout);
        // rectified window update for HP-style thresholding
        {
            float yr = std::max(0.0f, yout);
            rollWinRect_.push_back(yr);
            rollRectSum_ += yr;
            rollRectSumSq_ += static_cast<double>(yr) * static_cast<double>(yr);
        }
        while ((int)rollWin_.size() > winSamples_) {
            float u = rollWin_.front(); rollWin_.pop_front();
            rollSum_ -= u; rollSumSq_ -= static_cast<double>(u) * static_cast<double>(u);
        }
        while ((int)rollWinRect_.size() > winSamples_) {
            float u = rollWinRect_.front(); rollWinRect_.pop_front();
            rollRectSum_ -= u; rollRectSumSq_ -= static_cast<double>(u) * static_cast<double>(u);
        }
        // incremental local-max detection using 1-sample look-ahead
        if (dst >= 2) {
            float y2 = std::max(0.0f, filt_[dst - 2]);
            float y1 = std::max(0.0f, filt_[dst - 1]);
            float y0 = std::max(0.0f, filt_[dst - 0]);
            if (y1 > y2 && y1 >= y0) {
                int nwin = static_cast<int>(rollWinRect_.size());
                double mean = (nwin > 0 ? (rollRectSum_ / nwin) : 0.0);
                double var = (nwin > 0 ? (rollRectSumSq_ / nwin - mean * mean) : 0.0);
                if (var < 0.0) var = 0.0; double sd = std::sqrt(var);
                double thr;
                double y1Cmp = y1;
                if (hpThreshold_) {
                    double vmin = rectMinQ_.empty() ? y1 : rectMinQ_.front();
                    double vmax = rectMaxQ_.empty() ? y1 : rectMaxQ_.front();
                    double den = std::max(1e-6, vmax - vmin);
                    double scaledMean = (mean - vmin) / den * 1024.0;
                    const double effFsLocThr = (effectiveFs_ > 1e-6 ? effectiveFs_ : fs_);
                    size_t testAbs = firstAbs_ + (dst - 1);
                    double tnowThr = firstTsApprox_ + ((double)(testAbs - firstAbs_)) / effFsLocThr;
                    double lift = baseLift_ + ((tnowThr < tempLiftUntil_) ? tempLiftBoost_ : 0.0);
                    thr = scaledMean + lift;
                    y1Cmp = (y1 - vmin) / den * 1024.0;
                } else {
                    thr = mean + (opt_.thresholdScale * sd);
                }
                size_t absIdx = firstAbs_ + (dst - 1);
                if (y1Cmp > thr) {
                    // RR-predicted gating (timestamped path)
                    const double effFsLoc = (effectiveFs_ > 1e-6 ? effectiveFs_ : fs_);
                    bool allowPeak = true;
                    if (!peaksAbs_.empty()) {
                        size_t lastAbs = peaksAbs_.back();
                        double rr_new_ms = (double)(absIdx - lastAbs) / effFsLoc * 1000.0;
                        double tnow = firstTsApprox_ + ((double)(absIdx - firstAbs_)) / effFsLoc;
                        double bpm_prior = bpmEmaValid_ ? bpmEma_ : (0.5 * (opt_.bpmMin + opt_.bpmMax));
                        bpm_prior = std::max(opt_.bpmMin, std::min(opt_.bpmMax, bpm_prior));
                        double rr_prior_ms = std::max(opt_.minRRFloorRelaxed, std::min(opt_.minRRCeiling, 60000.0 / std::max(1e-6, bpm_prior)));
                        int acceptedRR = std::max(0, (int)acceptedPeaksTotal_ - 1);
                        bool gateRel = (tnow >= 15.0) && (acceptedRR >= 10) && (bpmEmaValid_ && bpmEma_ < 100.0);
                        double floor_ms = gateRel ? opt_.minRRFloorRelaxed : opt_.minRRFloorStrict;
                        double min_rr_ms = std::max(0.7 * rr_prior_ms, floor_ms);
                        if (rr_new_ms < min_rr_ms) {
                            size_t relLast = lastAbs >= firstAbs_ ? (lastAbs - firstAbs_) : 0;
                            float lastVal = (relLast < filt_.size() ? std::max(0.0f, filt_[relLast]) : y1);
                            double lastCmp = lastVal;
                            if (hpThreshold_) {
                                double vmin2 = y1, vmax2 = y1; for (float vv : rollWinRect_) { if (vv < vmin2) vmin2 = vv; if (vv > vmax2) vmax2 = vv; }
                                double den2 = std::max(1e-6, vmax2 - vmin2);
                                lastCmp = (lastVal - vmin2) / den2 * 1024.0;
                            }
                            double margin = gateRel ? 1.0 : 2.5;
                            if (!(y1Cmp > lastCmp + margin * sd)) allowPeak = false;
                        }
                        // dynamic refractory base tied to prior RR
                        int dynBaseRef = (int)std::lround(std::clamp(0.4 * rr_prior_ms, 280.0, 450.0) * 0.001 * effFsLoc);
                        // Rejection tracking and temporary lift/refractory bias
                        if (!allowPeak) {
                            if ((tnow - shortRejectWindowStart_) > 3.0) { shortRejectWindowStart_ = tnow; shortRejectCount_ = 0; }
                            ++shortRejectCount_;
                            if (shortRejectCount_ > 3) {
                                tempLiftBoost_ = std::max(tempLiftBoost_, 10.0);
                                tempLiftUntil_ = tnow + 2.0;
                                int capExtra = (int)std::lround(std::max(0.0, 0.35 - (opt_.refractoryMs * 0.001)) * effFsLoc);
                                dynRefExtraSamples_ = std::min(std::max(dynRefExtraSamples_, (int)std::lround(0.05 * effFsLoc)), capExtra);
                                dynRefUntil_ = tnow + 2.0;
                            }
                        }
                        if (tnow > dynRefUntil_) dynRefExtraSamples_ = 0;
                        // Track diagnostics for logging (applied refractory and min RR)
                        int appliedRef = dynBaseRef + dynRefExtraSamples_;
                        double tcur = firstTsApprox_ + ((double)(absIdx - firstAbs_)) / effFsLoc;
                        if (doublingActive_ && (tcur <= hardFallbackUntil_)) {
                            int fallbackRef = (int)std::lround(std::min(450.0, 0.5 * rr_prior_ms) * 0.001 * effFsLoc);
                            appliedRef = std::max(appliedRef, fallbackRef);
                        }
                        lastRefMsActive_ = appliedRef * 1000.0 / effFsLoc;
                        lastMinRRBoundMs_ = min_rr_ms;
                        // trough requirement between peaks
                        if (allowPeak) {
                            int start = (int)std::max((size_t)firstAbs_, lastAbs);
                            int end = (int)(absIdx);
                            double vmin2 = rectMinQ_.empty() ? y1 : rectMinQ_.front();
                            double vmax2 = rectMaxQ_.empty() ? y1 : rectMaxQ_.front();
                            double den2 = std::max(1e-6, vmax2 - vmin2);
                            double delta = 140.0;
                            double minCmp = 1e9;
                            for (int idx = start; idx < end; ++idx) {
                                int rel = idx - (int)firstAbs_;
                                if (rel < 0 || rel >= (int)filt_.size()) continue;
                                float yr2 = std::max(0.0f, filt_[rel]);
                                double cmp = (yr2 - vmin2) / den2 * 1024.0;
                                if (cmp < minCmp) minCmp = cmp;
                            }
                            if (!(minCmp < (thr - delta))) allowPeak = false;
                        }
                    }
                    if (allowPeak) {
                        if (peaksAbs_.empty()) {
                            peaksAbs_.push_back(absIdx);
                            ++acceptedPeaksTotal_;
                        } else {
                            size_t lastAbs = peaksAbs_.back();
                            // recompute dynamic base refractory here
                            double bpm_prior2 = bpmEmaValid_ ? bpmEma_ : (0.5 * (opt_.bpmMin + opt_.bpmMax));
                            double rr_prior_ms2 = std::max(400.0, std::min(1200.0, 60000.0 / std::max(1e-6, bpm_prior2)));
                            int baseRef2 = (int)std::lround(std::clamp(0.4 * rr_prior_ms2, 280.0, 450.0) * 0.001 * effFsLoc);
                            int refractoryNow = std::max(1, baseRef2) + dynRefExtraSamples_;
                            double tcur2 = firstTsApprox_ + ((double)(absIdx - firstAbs_)) / effFsLoc;
                            if (doublingActive_ && (tcur2 <= hardFallbackUntil_)) {
                                int fallbackRef = (int)std::lround(std::min(450.0, 0.5 * rr_prior_ms2) * 0.001 * effFsLoc);
                                refractoryNow = std::max(refractoryNow, fallbackRef);
                            }
                            if ((absIdx - lastAbs) >= (size_t)std::max(1, refractoryNow)) {
                                peaksAbs_.push_back(absIdx);
                                ++acceptedPeaksTotal_;
                            } else {
                                size_t relLast = lastAbs >= firstAbs_ ? (lastAbs - firstAbs_) : 0;
                                float lastVal = (relLast < filt_.size() ? std::max(0.0f, filt_[relLast]) : y1);
                                double lastCmp = lastVal;
                                if (hpThreshold_) {
                                    double vmin = y1, vmax = y1; for (float vv : rollWinRect_) { if (vv < vmin) vmin = vv; if (vv > vmax) vmax = vv; }
                                    double den2 = std::max(1e-6, vmax - vmin);
                                    lastCmp = (lastVal - vmin) / den2 * 1024.0;
                                }
                                if (y1Cmp > lastCmp) peaksAbs_.back() = absIdx;
                            }
                        }
                        // Update lastPeaks_/lastRR_ immediately
                        lastPeaks_.clear(); lastRR_.clear();
                        const double effFs = (effectiveFs_ > 1e-6 ? effectiveFs_ : fs_);
                        for (size_t j = 0; j < peaksAbs_.size(); ++j) {
                            size_t rel = peaksAbs_[j] - firstAbs_;
                            lastPeaks_.push_back(static_cast<int>(rel));
                            if (j > 0) {
                                double dts = static_cast<double>(peaksAbs_[j] - peaksAbs_[j - 1]) / effFs;
                                lastRR_.push_back(dts * 1000.0);
                            }
                        }
                        // Diagnostics already tracked above via appliedRef/min_rr_ms
                    }
                }
            }
        }
        ++totalAbs_;
    }
    // Rebuild display buffer decimation
    const double effFs = (effectiveFs_ > 1e-6 ? effectiveFs_ : fs_);
    int stride = std::max(1, (int)std::lround(effFs / std::max(10.0, displayHz_)));
    displayBuf_.clear(); displayBuf_.reserve(filt_.size() / stride + 1);
    for (size_t idx = 0; idx < filt_.size(); idx += (size_t)stride) displayBuf_.push_back(filt_[idx]);
    trimToWindow();
}

bool RealtimeAnalyzer::poll(HeartMetrics& out) {
    std::unique_lock<std::mutex> lock(dataMutex_);

    if ((lastTs_ - lastEmitTime_) < updateSec_) {
        return false;
    }
    lastEmitTime_ = lastTs_;

    // Step 1: copy the signal and timestamp windows in sync into reusable buffers
    if (pollWindowBuffer_.capacity() < filt_.size()) {
        pollWindowBuffer_.reserve(filt_.size());
    }
    if (pollTimestampBuffer_.capacity() < m_timestamps.size()) {
        pollTimestampBuffer_.reserve(m_timestamps.size());
    }
    pollWindowBuffer_.assign(filt_.begin(), filt_.end());
    pollTimestampBuffer_.assign(m_timestamps.begin(), m_timestamps.end());

    assert(
        pollWindowBuffer_.size() == pollTimestampBuffer_.size() &&
        "Signal and timestamp buffers must be in sync");

    double fsEff = (effectiveFs_ > 1e-6 ? effectiveFs_ : fs_);

    lock.unlock();

    // Step 2: analyze the signal window
    Options o = opt_;
    out = analyzeSignal(pollWindowBuffer_, fsEff, o);

    // Capture the analyzed waveform snapshot for downstream consumers
    out.waveform_values = pollWindowBuffer_;
    out.waveform_timestamps = pollTimestampBuffer_;

    // Step 3: map peak indices directly to timestamps from the synchronized window
    out.peakTimestamps.clear();
    if (!out.peakList.empty()) {
        out.peakTimestamps.reserve(out.peakList.size());
        for (int peak_index : out.peakList) {
            if (peak_index >= 0 &&
                static_cast<size_t>(peak_index) < pollTimestampBuffer_.size()) {
                out.peakTimestamps.push_back(
                    pollTimestampBuffer_[static_cast<size_t>(peak_index)]);
            }
        }
    }

    // Step 4: update SNR and quality
    updateSNR(out);

    lock.lock();
    lastQuality_ = out.quality;
    lock.unlock();

    return true;
}


} // namespace heartpy

// Plain C bridge
struct _hp_rt_handle { heartpy::RealtimeAnalyzer* p; };

void* hp_rt_create(double fs, const heartpy::Options* opt) {
    auto* h = new _hp_rt_handle();
    heartpy::Options o = opt ? *opt : heartpy::Options{};
    h->p = new heartpy::RealtimeAnalyzer(fs, o);
    return h;
}

void  hp_rt_set_window(void* h, double sec) {
    if (!h) return; auto* S = reinterpret_cast<_hp_rt_handle*>(h); S->p->setWindowSeconds(sec);
}

void  hp_rt_set_update_interval(void* h, double sec) {
    if (!h) return; auto* S = reinterpret_cast<_hp_rt_handle*>(h); S->p->setUpdateIntervalSeconds(sec);
}

void  hp_rt_push(void* h, const float* x, size_t n, double t0) {
    if (!h || !x || n == 0) return;
    auto* S = reinterpret_cast<_hp_rt_handle*>(h);
    S->p->push(x, n, t0); // push() clamps batch size internally
}

void  hp_rt_push_ts(void* h, const float* x, const double* ts, size_t n) {
    if (!h || !x || !ts || n == 0) return;
    auto* S = reinterpret_cast<_hp_rt_handle*>(h);
    S->p->push(x, ts, n);
}

int   hp_rt_poll(void* h, heartpy::HeartMetrics* out) {
    if (!h || !out) return 0; auto* S = reinterpret_cast<_hp_rt_handle*>(h); return S->p->poll(*out) ? 1 : 0;
}

void  hp_rt_destroy(void* h) {
    if (!h) return; auto* S = reinterpret_cast<_hp_rt_handle*>(h); delete S->p; delete S;
}

namespace heartpy {

double RealtimeAnalyzer::medianOfRR(const std::vector<double>& rr) {
    if (rr.empty()) return 0.0;
    scratchRR_.assign(rr.begin(), rr.end());
    auto mid = scratchRR_.begin() + scratchRR_.size() / 2;
    std::nth_element(scratchRR_.begin(), mid, scratchRR_.end());
    return *mid;
}

void RealtimeAnalyzer::updateSNR(HeartMetrics& out) {
    const double sinceLastPsd = lastTs_ - lastPsdTime_;
    if (sinceLastPsd < psdUpdateSec_) {
        out.quality = lastQuality_;
        out.quality.snrSampleCount = static_cast<double>(filt_.size());
        LOGD("updateSNR cadence skip: dt=%.3f < %.3f, reuse previous quality (snr=%.3f)", sinceLastPsd, psdUpdateSec_, out.quality.snrDb);
        return;
    }
    lastPsdTime_ = lastTs_;

    // Use full-rate filtered window for PSD and derive SNR around HR
    const double effFs = (effectiveFs_ > 1e-6 ? effectiveFs_ : fs_);
    const size_t sampleCount = filt_.size();
    LOGD("updateSNR: effFs=%.3f, filt_.size()=%zu, fs_=%.3f", effFs, sampleCount, fs_);
    out.quality.snrSampleCount = static_cast<double>(sampleCount);
    if (effFs <= 0.0 || sampleCount < 16) {
        LOGD("Early return: effFs=%.3f <= 0.0 OR filt_.size()=%zu < 16", effFs, sampleCount);
        double fallbackDb = snrEmaValid_ ? snrEmaDb_ : kSnrFallbackDb;
        if (!std::isfinite(fallbackDb)) fallbackDb = kSnrFallbackDb;
        out.quality.snrDb = fallbackDb;
        out.quality.hardFallbackActive = 1;
        out.quality.snrWarmupActive = 1;
        return;
    }

    // Estimate HR frequency f0 (Hz) from streaming RR if available; fallback to out.bpm; reuse last if missing
    double f0 = 0.0;
    if (!out.rrList.empty()) {
        double mrr = 0.0; for (double r : out.rrList) mrr += r; mrr /= (double)out.rrList.size();
        if (mrr > 1e-3) f0 = 1000.0 / mrr; // ms -> Hz
    }
    if (f0 <= 0.0 && out.bpm > 0.0) f0 = out.bpm / 60.0;
    if (f0 <= 0.0 && lastF0Hz_ > 0.0) f0 = lastF0Hz_;
    // If no HR estimate, skip SNR update
    if (f0 <= 0.0) {
        LOGD("Early return: f0 <= 0.0 (f0=%.6f)", f0);
        // Set a fallback SNR value instead of returning
        double fallbackDb = snrEmaValid_ ? snrEmaDb_ : kSnrFallbackDb;
        if (!std::isfinite(fallbackDb)) fallbackDb = kSnrFallbackDb;
        out.quality.snrDb = fallbackDb;
        out.quality.f0Hz = 0.0;
        out.quality.hardFallbackActive = 1;
        return;
    }
    lastF0Hz_ = f0;

    // Build analysis vector (copy of filt_) — reuse buffer to reduce reallocations
    yBufferD_.resize(filt_.size());
    for (size_t i = 0; i < filt_.size(); ++i) yBufferD_[i] = (double)filt_[i];
    LOGD("yBufferD_.size(): %zu, filt_.size(): %zu", yBufferD_.size(), filt_.size());

    // Welch PSD on the full-rate filtered signal
    struct WelchConfig {
        int nfft;
        double overlap;
        int nseg;
        bool adjusted;
    };

    auto largestPowerOfTwoLE = [](size_t value) -> int {
        if (value < 1) return 0;
        size_t pow2 = 1;
        while ((pow2 << 1) <= value) {
            pow2 <<= 1;
        }
        return static_cast<int>(pow2);
    };

    auto coerceNfft = [&](int n) -> int {
        if (n <= 0) return 256;
        int candidates[] = {1024, 512, 384, 256, 192, 128, 96, 64, 48, 32};
        int best = candidates[sizeof(candidates)/sizeof(candidates[0]) - 1];
        int bestd = std::numeric_limits<int>::max();
        for (int cand : candidates) {
            if (cand < 32) continue;
            int d = std::abs(n - cand);
            if (d < bestd) { bestd = d; best = cand; }
        }
        return best;
    };

    auto chooseWelchConfig = [&](size_t sampleCount) -> std::optional<WelchConfig> {
        constexpr int kMinNfft = 32;
        if (sampleCount < static_cast<size_t>(kMinNfft)) {
            return std::nullopt;
        }
        double baseOverlap = std::clamp(opt_.overlap, 0.0, 0.90);
        int desired = coerceNfft(opt_.nfft);
        desired = std::min(desired, largestPowerOfTwoLE(sampleCount));
        desired = std::max(desired, kMinNfft);

        int workingNfft = desired;
        double workingOverlap = baseOverlap;
        bool adjusted = false;

        while (workingNfft >= kMinNfft) {
            if (workingNfft > static_cast<int>(sampleCount)) {
                int next = largestPowerOfTwoLE(sampleCount);
                if (next < kMinNfft) break;
                workingNfft = next;
                adjusted = true;
                continue;
            }
            if (static_cast<size_t>(workingNfft) >= sampleCount) {
                if (workingNfft == kMinNfft) break;
                int next = largestPowerOfTwoLE(static_cast<size_t>(workingNfft - 1));
                if (next < kMinNfft) break;
                workingNfft = next;
                adjusted = true;
                continue;
            }

            double minOverlapForTwo = 1.0 - (static_cast<double>(sampleCount - workingNfft) / static_cast<double>(workingNfft));
            minOverlapForTwo = std::clamp(minOverlapForTwo, 0.0, 0.93);
            double overlapCandidate = std::max(workingOverlap, minOverlapForTwo + 0.02);
            overlapCandidate = std::clamp(overlapCandidate, baseOverlap, 0.93);

            double stepFloat = static_cast<double>(workingNfft) * (1.0 - overlapCandidate);
            if (stepFloat < 1.0) stepFloat = 1.0;
            int step = std::max(1, static_cast<int>(std::round(stepFloat)));
            int nseg = 1 + static_cast<int>((sampleCount - workingNfft) / step);
            if (nseg >= 2) {
                if (std::fabs(overlapCandidate - baseOverlap) > 1e-6 || workingNfft != desired) {
                    adjusted = true;
                }
                return WelchConfig{workingNfft, overlapCandidate, nseg, adjusted};
            }

            if (overlapCandidate < 0.93 - 1e-6) {
                workingOverlap = std::min(0.93, overlapCandidate + 0.05);
                adjusted = true;
                continue;
            }

            if (workingNfft == kMinNfft) break;
            int next = largestPowerOfTwoLE(static_cast<size_t>(workingNfft - 1));
            if (next < kMinNfft) break;
            workingNfft = next;
            adjusted = true;
        }

        return std::nullopt;
    };

    enum class SnrSource { FreshPsd, CachedPsd, TimeDomain };
    SnrSource snrSource = SnrSource::FreshPsd;
    bool harmonicEligible = false;
    const std::vector<double>* freqBins = nullptr;
    const std::vector<double>* powerBins = nullptr;
    int nfft = coerceNfft(opt_.nfft);
    double overlapForCall = opt_.overlap;

    std::optional<WelchConfig> welchConfig;
    if (opt_.adaptivePsd) {
        welchConfig = chooseWelchConfig(yBufferD_.size());
    } else {
        WelchConfig preset{
            coerceNfft(opt_.nfft),
            std::clamp(opt_.overlap, 0.0, 0.90),
            0,
            false,
        };
        if (preset.nfft > static_cast<int>(yBufferD_.size())) {
            int fallbackNfft = largestPowerOfTwoLE(yBufferD_.size());
            preset.nfft = (fallbackNfft >= 32) ? fallbackNfft : 0;
        }
        if (preset.nfft >= 32) {
            welchConfig = preset;
        }
    }

    if (!welchConfig.has_value()) {
        ++psdInvalidFramesTotal_;
        if (opt_.adaptivePsd) {
            LOGD("Insufficient data for Welch PSD (samples=%zu). Falling back to time-domain SNR", yBufferD_.size());
            snrSource = SnrSource::TimeDomain;
            lastPsdValid_ = false;
        } else {
            LOGD("Insufficient data for Welch PSD (adaptive disabled, samples=%zu). Skipping SNR update", yBufferD_.size());
            return;
        }
    } else {
        if (welchConfig->adjusted) {
            ++psdParamClampEventsTotal_;
            LOGD("Welch params adjusted: nfft=%d, overlap=%.3f, nseg=%d", welchConfig->nfft, welchConfig->overlap, welchConfig->nseg);
        }
        nfft = welchConfig->nfft;
        overlapForCall = welchConfig->overlap;
        LOGD("WelchPSD input: signal.size()=%zu, fs=%.3f, nfft=%d, overlap=%.3f, nseg=%d", yBufferD_.size(), effFs, nfft, overlapForCall, welchConfig->nseg);
        heartpy::setDeterministic(opt_.deterministic);
        auto ps = welchPowerSpectrum(yBufferD_, effFs, nfft, overlapForCall);
        const auto& frq = ps.first;
        const auto& P = ps.second;
        LOGD("PSD calculation: frq.size()=%zu, P.size()=%zu", frq.size(), P.size());
        if (frq.size() >= 4 && frq.size() == P.size()) {
            lastPsdFreq_ = frq;
            lastPsdPower_ = P;
            lastPsdFs_ = effFs;
            lastPsdNfft_ = nfft;
            lastPsdOverlap_ = overlapForCall;
            lastPsdValid_ = true;
            freqBins = &lastPsdFreq_;
            powerBins = &lastPsdPower_;
            harmonicEligible = true;
        } else {
            ++psdInvalidFramesTotal_;
            LOGD("PSD validation failed (frq.size()=%zu, P.size()=%zu)", frq.size(), P.size());
            if (!opt_.adaptivePsd) {
                LOGD("Adaptive PSD disabled; aborting SNR update after invalid PSD");
                return;
            }
            if (lastPsdValid_ && lastPsdFreq_.size() >= 4 && lastPsdFreq_.size() == lastPsdPower_.size()) {
                freqBins = &lastPsdFreq_;
                powerBins = &lastPsdPower_;
                snrSource = SnrSource::CachedPsd;
                ++psdReuseFallbackEventsTotal_;
                LOGD("Reusing cached PSD (bins=%zu, last nfft=%d, overlap=%.3f)", lastPsdFreq_.size(), lastPsdNfft_, lastPsdOverlap_);
            } else {
                snrSource = SnrSource::TimeDomain;
                lastPsdValid_ = false;
            }
        }
    }

    auto computeTimeDomainSnrDb = [](const std::vector<double>& samples) -> double {
        if (samples.size() < 16) {
            return kSnrFallbackDb;
        }
        double mean = 0.0;
        for (double v : samples) mean += v;
        mean /= static_cast<double>(samples.size());
        double signalVar = 0.0;
        for (double v : samples) {
            double d = v - mean;
            signalVar += d * d;
        }
        signalVar /= std::max<size_t>(1, samples.size() - 1);
        if (signalVar <= 1e-10) {
            return kSnrFallbackDb;
        }
        double diffVar = 0.0;
        for (size_t i = 1; i < samples.size(); ++i) {
            double d = samples[i] - samples[i - 1];
            diffVar += d * d;
        }
        diffVar /= std::max<size_t>(1, samples.size() - 1);
        double noiseVar = std::max(1e-10, diffVar * 0.5);
        double ratio = signalVar / noiseVar;
        double snrDb = 10.0 * std::log10(std::max(1e-10, ratio));
        if (!std::isfinite(snrDb)) snrDb = kSnrFallbackDb;
        return snrDb;
    };

    auto inBand = [](double f, double c, double bw){ return std::fabs(f - c) <= bw; };
    double signalPow = 0.0;
    double noiseBaseline = 0.0;
    double band = 0.0;
    double df = 0.0;
    double snrDbInst = kSnrFallbackDb;
    bool activeSnr = false;
    double baseBw = opt_.snrBandPassive;
    double warmupSec = std::clamp(windowSec_ * 0.6, 6.0, 18.0);
    double warmupElapsed = std::isfinite(warmupStartTs_)
        ? std::max(0.0, lastTs_ - warmupStartTs_)
        : std::max(0.0, lastTs_ - firstTsApprox_);
    size_t minSamplesForSNR = static_cast<size_t>(std::ceil(std::max(128.0, std::max(4.0, windowSec_ * 0.6) * effFs)));
    size_t minPeaksForSNR = std::max<size_t>(6, static_cast<size_t>(std::ceil(windowSec_ * 0.4)));
    bool insufficientPeaks = acceptedPeaksTotal_ < minPeaksForSNR;
    bool warmupActive = (warmupElapsed < warmupSec) || (sampleCount < minSamplesForSNR) || insufficientPeaks;
    LOGD("updateSNR warmup check: elapsed=%.3f sec, warmupSec=%.3f sec, windowSec=%.3f, sampleCount=%zu, minSamples=%zu, acceptedPeaks=%zu, warmupActive=%d",
         warmupElapsed, warmupSec, windowSec_, sampleCount, minSamplesForSNR, acceptedPeaksTotal_, warmupActive ? 1 : 0);

    if (warmupActive) {
        double warmSnr = snrEmaValid_ ? snrEmaDb_ : computeTimeDomainSnrDb(yBufferD_);
        if (!std::isfinite(warmSnr) || warmSnr <= 0.0) warmSnr = 8.0;
        snrEmaDb_ = warmSnr;
        snrEmaValid_ = true;
        out.quality.snrDb = warmSnr;
        out.quality.f0Hz = lastF0Hz_;
        out.quality.snrWarmupActive = 1;
        out.quality.hardFallbackActive = 0;
        return;
    }
    out.quality.snrWarmupActive = 0;

    if (snrSource == SnrSource::TimeDomain) {
        snrDbInst = computeTimeDomainSnrDb(yBufferD_);
        ++psdTimeDomainFallbackEventsTotal_;
        LOGD("Time-domain SNR fallback applied: %.3f dB", snrDbInst);
    } else {
        const auto& frq = *freqBins;
        const auto& P = *powerBins;
        double freqMin = frq.empty() ? 0.0 : frq.front();
        double freqMax = frq.empty() ? 0.0 : frq.back();
        df = (frq.size() > 1 ? frq[1] - frq[0] : 0.0);
        double nyq = 0.5 * effFs;
        LOGD("Using %s PSD (bins=%zu) for SNR computation", snrSource == SnrSource::FreshPsd ? "fresh" : "cached", frq.size());
        LOGD("PSD frequency span: %.4f Hz -> %.4f Hz (df=%.6f, nyquist=%.3f)", freqMin, freqMax, df, nyq);

        double lastActiveTs = 0.0;
        if (softLastTrueTs_ > 0.0) lastActiveTs = std::max(lastActiveTs, softLastTrueTs_);
        if (doublingLastTrueTs_ > 0.0) lastActiveTs = std::max(lastActiveTs, doublingLastTrueTs_);
        if (hintLastTrueTs_ > 0.0) lastActiveTs = std::max(lastActiveTs, hintLastTrueTs_);
        bool persistMapLoc = (lastActiveTs > 0.0) && ((lastTs_ - lastActiveTs) <= 5.0);
        activeSnr = doublingHintActive_ || softDoublingActive_ || doublingActive_ || persistMapLoc;
        baseBw = activeSnr ? opt_.snrBandActive : opt_.snrBandPassive;
        band = std::max(2.0 * df, baseBw);
        double guard = 0.03;
        double peakPow = 0.0;
        double peakPow2 = 0.0;
        noiseScratch_.clear();
        noiseScratch_.reserve(frq.size());
        double bandLoFund = std::max(0.0, f0 - band);
        double bandHiFund = f0 + band;
        double bandLoHarm = (2.0 * f0 < nyq) ? std::max(0.0, 2.0 * f0 - band) : 0.0;
        double bandHiHarm = (2.0 * f0 < nyq) ? (2.0 * f0 + band) : 0.0;
        LOGD("Signal band (fundamental): %.4f Hz -> %.4f Hz", bandLoFund, bandHiFund);
        if (2.0 * f0 < nyq) {
            LOGD("Signal band (harmonic): %.4f Hz -> %.4f Hz", bandLoHarm, bandHiHarm);
        }
        for (size_t i = 0; i < frq.size(); ++i) {
            double f = frq[i];
            double pv = std::abs(P[i]);
            bool sig1 = inBand(f, f0, band);
            bool sig2 = (2.0 * f0 < nyq) && inBand(f, 2.0 * f0, band);
            if (sig1) peakPow += pv;
            if (sig2) peakPow2 += pv;
            bool nearSig = inBand(f, f0, band + guard) || ((2.0 * f0 < nyq) && inBand(f, 2.0 * f0, band + guard));
            if (!nearSig && f >= 0.4 && f <= 5.0) noiseScratch_.push_back(pv);
        }
        LOGD("noiseScratch population: %zu (after exclusions)", noiseScratch_.size());
        if (noiseScratch_.empty()) {
            LOGD("Noise candidate window empty; guard=%.3f, evaluation band=%.3f-%.3f Hz", guard, bandLoFund, bandHiFund);
        }
        LOGD("peak power fundamental=%.6e, harmonic=%.6e", peakPow, peakPow2);
        signalPow = peakPow + peakPow2;
        if (!noiseScratch_.empty()) {
            const size_t n = noiseScratch_.size();
            std::sort(noiseScratch_.begin(), noiseScratch_.end());
            const size_t startIdx = n / 20;
            const size_t endIdx = n - startIdx;
            if (endIdx > startIdx) {
                const size_t p75Idx = startIdx + (endIdx - startIdx) * 3 / 4;
                noiseBaseline = std::max(noiseScratch_[p75Idx], 1e-8);
            }
        }
        LOGD("f0: %.3f", f0);
        LOGD("signalPow: %.6f", signalPow);
        LOGD("noiseBaseline: %.6f", noiseBaseline);
        LOGD("band: %.6f, df: %.6f", band, df);
        LOGD("noiseScratch_.size(): %zu", noiseScratch_.size());

        if (signalPow > 1e-10 && noiseBaseline > 1e-10) {
            LOGD("Signal power threshold passed: signalPow=%.6e > 1e-10, noiseBaseline=%.6e > 1e-10", signalPow, noiseBaseline);
            double noiseBandwidth = band * 2.0 / std::max(1e-6, df);
            if (noiseBandwidth > 1e-6) {
                double snrRatio = signalPow / (noiseBaseline * noiseBandwidth);
                if (snrRatio > 1e-10) {
                    double candidate = 10.0 * std::log10(snrRatio);
                    if (std::isfinite(candidate)) {
                        snrDbInst = candidate;
                    }
                }
            }
        } else {
            LOGD("Signal power threshold failed: signalPow=%.6e <= 1e-10 OR noiseBaseline=%.6e <= 1e-10", signalPow, noiseBaseline);
        }
    }
    double snrDbInstRaw = snrDbInst;
    LOGD("snrDbInst (before clamp): %.3f", snrDbInstRaw);
    if (!std::isfinite(snrDbInst)) snrDbInst = kSnrFallbackDb;
    LOGD("snrDbInst (after clamp): %.3f", snrDbInst);
    // EMA smoothing over time (tau = 8s when active)
    double now = lastTs_;
    double dt = (lastSnrUpdateTime_ > 0.0) ? (now - lastSnrUpdateTime_) : psdUpdateSec_;
    if (opt_.deterministic) dt = psdUpdateSec_;
    double tau = activeSnr ? opt_.snrActiveTauSec : snrTauSec_;
    double alpha = 1.0 - std::exp(-dt / std::max(1e-3, tau));
    if (!snrEmaValid_) { snrEmaDb_ = snrDbInst; snrEmaValid_ = true; }
    else {
        snrEmaDb_ = (1.0 - alpha) * snrEmaDb_ + alpha * snrDbInst;
    }
    // Blend toward instant value when band mode or width changes to avoid step bias
    bool bandWidthChanged = (std::fabs(baseBw - lastSnrBaseBw_) > 1e-9) || (activeSnr != lastSnrActiveMode_);
    if (bandWidthChanged && !opt_.deterministic) {
        double bf = std::clamp(opt_.snrBandBlendFactor, 0.0, 1.0);
        snrEmaDb_ = (1.0 - bf) * snrEmaDb_ + bf * snrDbInst;
    }
    lastSnrBaseBw_ = baseBw; lastSnrActiveMode_ = activeSnr;
    lastSnrUpdateTime_ = now;
    if (!std::isfinite(snrEmaDb_)) snrEmaDb_ = kSnrFallbackDb;
    out.quality.snrDb = snrEmaDb_;
    out.quality.f0Hz = lastF0Hz_;

    // Debug: Log SNR calculation details (only when SNR changes significantly)
    static double lastLoggedSnr = 999.0;
    if (std::abs(snrEmaDb_ - lastLoggedSnr) > 1.0 || snrEmaDb_ > 5.0) {
        // Note: In production, this could be replaced with proper logging
        // For now, we rely on JS-side logging
        lastLoggedSnr = snrEmaDb_;
    }

    double f0Half = 0.5 * lastF0Hz_;
    double pFund = 0.0;
    double pHalf = 0.0;
    double shortFrac = 0.0;
    double longRR = 0.0;
    double rrCV = 0.0;
    double pairFrac = 0.0;
    double shortMean = 0.0;
    double longMean = 0.0;
    double ratioHalfFund = 0.0;
    bool halfStable = false;

    int acceptedRR = std::max(0, (int)acceptedPeaksTotal_ - 1);
    bool warmupPassed = ((lastTs_ - firstTsApprox_) >= 15.0) && (acceptedRR >= 10);

    if (harmonicEligible && freqBins && powerBins) {
        const auto& frqForHarm = *freqBins;
        const auto& powForHarm = *powerBins;
        if (lastF0Hz_ > 0.0) {
            for (size_t i = 0; i < frqForHarm.size(); ++i) {
                double f = frqForHarm[i];
                double pv = std::abs(powForHarm[i]);
                if (inBand(f, lastF0Hz_, band)) pFund += pv;
                if (f0Half > 0.0 && inBand(f, f0Half, band)) pHalf += pv;
            }
        }
        if (!out.rrList.empty()) {
            std::vector<double> rr = out.rrList;
            std::vector<double> tmp = rr;
            std::nth_element(tmp.begin(), tmp.begin() + tmp.size() / 2, tmp.end());
            double med = tmp[tmp.size() / 2];
            double thr = 0.8 * med;
            double sumLong = 0.0, sumShort = 0.0; int cntLong = 0, cntShort = 0;
            for (double r : rr) { if (r >= thr) { sumLong += r; ++cntLong; } else { sumShort += r; ++cntShort; } }
            if (cntLong > 0) longRR = sumLong / cntLong; else longRR = med;
            longMean = (cntLong > 0 ? (sumLong / cntLong) : med);
            shortMean = (cntShort > 0 ? (sumShort / cntShort) : 0.0);
            shortFrac = (rr.size() > 0 ? (cntShort / (double)rr.size()) : 0.0);
            double mean_rr = meanVec(rr);
            double var_rr = 0.0; for (double r : rr) { double d = r - mean_rr; var_rr += d * d; }
            var_rr /= (double)rr.size(); rrCV = (mean_rr > 1e-9) ? std::sqrt(std::max(0.0, var_rr)) / mean_rr : 0.0;
            int cntPairs = 0, goodPairs = 0;
            for (size_t i = 0; i + 1 < rr.size(); ++i) {
                double s = rr[i] + rr[i + 1];
                if (longRR > 0.0) {
                    ++cntPairs;
                    if (s >= 0.85 * longRR && s <= 1.15 * longRR) ++goodPairs;
                }
            }
            pairFrac = (cntPairs > 0 ? (goodPairs / (double)cntPairs) : 0.0);
        }
        ratioHalfFund = (pFund > 0.0 ? (pHalf / pFund) : 0.0);
        LOGD("pHalf: %.6f, pFund: %.6f, ratioHalfFund: %.6f", pHalf, pFund, ratioHalfFund);

        int halfLen = std::max(2, opt_.halfF0HistLen);
        if (f0Half > 0.0) { halfF0Hist_.push_back(f0Half); if ((int)halfF0Hist_.size() > halfLen) halfF0Hist_.pop_front(); }
        else halfF0Hist_.clear();
        double driftTol = warmupPassed ? opt_.halfF0TolHzWarm : opt_.halfF0TolHzCold;
        halfStable = false; if (halfF0Hist_.size() >= 2) { double fmin = *std::min_element(halfF0Hist_.begin(), halfF0Hist_.end()); double fmax = *std::max_element(halfF0Hist_.begin(), halfF0Hist_.end()); halfStable = ((fmax - fmin) <= driftTol); }
        bool softGuards = (out.quality.rejectionRate <= 0.05) && (rrCV <= 0.30) && warmupPassed;
        if (warmupPassed && !warmupWasPassed_) { softConsecPass_ = 0; halfF0Hist_.clear(); }
        warmupWasPassed_ = warmupPassed;
        LOGD("warmupPassed: %d, halfStable: %d, rejectionRate: %.4f, rrCV: %.4f", warmupPassed ? 1 : 0, halfStable ? 1 : 0, out.quality.rejectionRate, rrCV);
    // Immediate soft activation post warm-up on PSD dominance (no streak requirement)
    bool softPass = warmupPassed && (ratioHalfFund >= opt_.pHalfOverFundThresholdSoft) && halfStable && softGuards;
    if (softPass) {
        LOGD("softPass triggered");
        if (!softDoublingActive_) softStartTs_ = lastTs_;
        softDoublingActive_ = true;
        softConsecPass_ = 2; // for logging
        softLastTrueTs_ = lastTs_;
    } else {
        softConsecPass_ = 0;
        // Only keep soft active if hard doubling is governing
        if (!doublingActive_) softDoublingActive_ = false;
    }
        // Stage 2 hard flag check
        bool persistHighBpm = (bpmEmaValid_ && bpmEma_ > 120.0 && out.quality.maPercActive < 25.0);
        bool psdPersists = (ratioHalfFund >= 2.0) && halfStable;
        LOGD("softDoublingActive_: %d, doublingActive_: %d, doublingHintActive_: %d", softDoublingActive_ ? 1 : 0, doublingActive_ ? 1 : 0, doublingHintActive_ ? 1 : 0);
        bool hardStable = (out.quality.rejectionRate <= 0.05) && (rrCV <= 0.20);
        LOGD("psdPersists: %d, hardStable: %d", psdPersists ? 1 : 0, hardStable ? 1 : 0);
    if (softDoublingActive_ && ((lastTs_ - softStartTs_) >= 8.0) && psdPersists && persistHighBpm && hardStable) {
        doublingActive_ = true;
        doublingHoldUntil_ = std::max(doublingHoldUntil_, lastTs_ + 5.0);
        doublingLastTrueTs_ = lastTs_;
        if (longRR > 0.0) doublingLongRRms_ = longRR;
        // Bound hard fallback window to ≤3s and within hold window
        double hardRemain = std::max(0.0, doublingHoldUntil_ - lastTs_);
        hardFallbackUntil_ = lastTs_ + std::min(3.0, hardRemain);
    }
    bool hardGuardsOk = (ratioHalfFund >= 1.5) && halfStable && (out.quality.rejectionRate <= 0.05) && (rrCV <= 0.20);
    if (doublingActive_) { if (hardGuardsOk) doublingLastTrueTs_ = lastTs_; if ((lastTs_ - doublingLastTrueTs_) >= 5.0 && lastTs_ >= doublingHoldUntil_) doublingActive_ = false; }
    // Oversuppression (choke) protection: if active doubling and BPM (from RR median) < 40 for >3s after 20s
    {
        double bpmEst = 0.0;
        if (!out.rrList.empty()) {
            std::vector<double> tmp = out.rrList; std::nth_element(tmp.begin(), tmp.begin() + tmp.size()/2, tmp.end());
            double med = tmp[tmp.size()/2]; if (med > 1e-6) bpmEst = 60000.0 / med;
        }
        bool dblActive = (doublingHintActive_ || softDoublingActive_ || doublingActive_);
        if (dblActive && (lastTs_ >= 20.0) && (bpmEst > 0.0 && bpmEst < opt_.chokeBpmThreshold)) {
            if (chokeStartTs_ <= 0.0) chokeStartTs_ = lastTs_;
            if ((lastTs_ - chokeStartTs_) >= 3.0) {
                double recoveryTime = (bpmEst < opt_.chokeBpmThreshold) ? opt_.chokeRelaxLowBpmSec : opt_.chokeRelaxBaseSec;
                chokeRelaxUntil_ = lastTs_ + recoveryTime; // adaptive relax
            }
        } else {
            chokeStartTs_ = 0.0;
        }
    }
    // Doubling hint (post warm-up): PSD path or RR-centric fallback under conservative guards
    bool psdHintPass = warmupPassed && (ratioHalfFund >= opt_.pHalfOverFundThresholdSoft) && halfStable && (out.quality.rejectionRate <= 0.05) && (rrCV <= 0.30);
    // Optional subdominant PSD fallback (>=1.6 for ~6s, slightly looser drift)
    bool halfStableLoose = false; if (halfF0Hist_.size() >= 2) { double fmin2 = *std::min_element(halfF0Hist_.begin(), halfF0Hist_.end()); double fmax2 = *std::max_element(halfF0Hist_.begin(), halfF0Hist_.end()); halfStableLoose = ((fmax2 - fmin2) <= 0.08); }
    static double psdLoStart = 0.0;
    bool psdLoNow = warmupPassed && (ratioHalfFund >= opt_.pHalfOverFundThresholdLow) && halfStableLoose && (out.quality.rejectionRate <= 0.05) && (rrCV <= 0.20);
    bool psdLoHold = false;
    if (psdLoNow) { if (psdLoStart <= 0.0) psdLoStart = lastTs_; if ((lastTs_ - psdLoStart) >= 6.0) psdLoHold = true; }
    else { psdLoStart = 0.0; }
    // RR-centric fallback: sustained high BPM, clean & stable RR around ~150 BPM (short mode)
    double medRR = 0.0; if (!out.rrList.empty()) { std::vector<double> tmp=out.rrList; std::nth_element(tmp.begin(), tmp.begin()+tmp.size()/2, tmp.end()); medRR = tmp[tmp.size()/2]; }
    bool rrBand = (medRR >= 370.0 && medRR <= 450.0);
    bool highBpmPersist = bpmHighActive_ && ((lastTs_ - std::max(0.0, bpmHighStartTs_)) >= 8.0);
    bool rrClean = (rrCV <= 0.10) && (out.quality.rejectionRate <= 0.03);
    bool rrFallbackNow = warmupPassed && highBpmPersist && rrClean && rrBand;
    if (rrFallbackNow) ++rrFallbackConsec_; else rrFallbackConsec_ = 0;
    bool rrHintPass = (rrFallbackConsec_ >= 3);

    rrFallbackActive_ = rrHintPass; // mark whether RR path triggered this poll
    if (psdHintPass || psdLoHold || rrHintPass) {
        double hold = psdHintPass ? 12.0 : 8.0;
        if (!doublingHintActive_) { hintHoldUntil_ = lastTs_ + hold; hintStartTs_ = lastTs_; }
        doublingHintActive_ = true;
        hintLastTrueTs_ = lastTs_;
        lastHintBadStart_ = 0.0;
        // Track whether hint is driven by RR fallback only (not PSD)
        bool rrOnly = rrHintPass && !(psdHintPass || psdLoHold);
        if (rrOnly) rrFallbackDrivingHint_ = true;
    } else {
        // violation tracking similar to auto-clear: close after 2s of violations (but not before hold)
        if (doublingHintActive_) {
            if (lastHintBadStart_ <= 0.0) lastHintBadStart_ = lastTs_;
            if ((lastTs_ - lastHintBadStart_) >= 2.0 && lastTs_ >= hintHoldUntil_) doublingHintActive_ = false;
        }
    }
        if (!doublingHintActive_) rrFallbackDrivingHint_ = false;
        rrFallbackModeActive_ = rrFallbackDrivingHint_;
    } else {
        LOGD("Skipping harmonic suppression update: PSD not valid this frame (warmup=%d)", warmupPassed ? 1 : 0);
        warmupWasPassed_ = warmupPassed;
    }

    // Choose f0 used for SNR/conf
    // Auto-clear: if violation persists ≥5s, drop both flags
    bool clearViolate = (ratioHalfFund < 1.5) || (!halfStable) || (rrCV > 0.20) || (out.quality.rejectionRate > 0.05);
    if (clearViolate) {
        if (lastClearBadStart_ <= 0.0) lastClearBadStart_ = lastTs_;
        if ((lastTs_ - lastClearBadStart_) >= 5.0) { softDoublingActive_ = false; doublingActive_ = false; }
    } else {
        lastClearBadStart_ = 0.0;
    }
    bool halfDominant = (ratioHalfFund >= opt_.pHalfOverFundThresholdSoft) && halfStable;
    // Keep mapping to 1/2 f0 for 5s after last active to stabilize SNR/conf
    double lastActiveTs_map = 0.0;
    if (softLastTrueTs_ > 0.0) lastActiveTs_map = std::max(lastActiveTs_map, softLastTrueTs_);
    if (doublingLastTrueTs_ > 0.0) lastActiveTs_map = std::max(lastActiveTs_map, doublingLastTrueTs_);
    if (hintLastTrueTs_ > 0.0) lastActiveTs_map = std::max(lastActiveTs_map, hintLastTrueTs_);
    bool persistMap = (lastActiveTs_map > 0.0) && ((lastTs_ - lastActiveTs_map) <= 5.0);
    bool useHalfForSNR = softDoublingActive_ || doublingActive_ || doublingHintActive_ || halfDominant || persistMap;
    double f0Used = f0;
    if (useHalfForSNR && f0 > 0.0) {
        double signalPowUsed = pHalf + pFund; // half fundamental + original f0
        double snrDbInst2 = kSnrFallbackDb;
        if (signalPowUsed > 0.0 && noiseBaseline > 0.0) {
            double bw2 = band * 2.0 / std::max(1e-6, df);
            if (bw2 > 1e-6) {
                double ratio2 = signalPowUsed / (noiseBaseline * bw2);
                if (ratio2 > 1e-10) {
                    double candidate2 = 10.0 * std::log10(ratio2);
                    if (std::isfinite(candidate2)) {
                        snrDbInst2 = candidate2;
                    }
                }
            }
        }
        double snrDbInst2Raw = snrDbInst2;
        LOGD("snrDbInst2 (before clamp): %.3f", snrDbInst2Raw);
        if (!std::isfinite(snrDbInst2)) snrDbInst2 = kSnrFallbackDb;
        LOGD("snrDbInst2 (after clamp): %.3f", snrDbInst2);
        if (!snrEmaValid_) { snrEmaDb_ = snrDbInst2; snrEmaValid_ = true; }
        else snrEmaDb_ = (1.0 - alpha) * snrEmaDb_ + alpha * snrDbInst2;
        f0Used = 0.5 * f0;
    }
    lastF0Hz_ = f0Used; out.quality.f0Hz = lastF0Hz_; out.quality.snrDb = snrEmaDb_;
    out.quality.softDoublingFlag = softDoublingActive_ ? 1 : 0;
    out.quality.doublingFlag = doublingActive_ ? 1 : 0;
    out.quality.hardFallbackActive = (doublingActive_ && (lastTs_ <= hardFallbackUntil_)) ? 1 : 0;
    out.quality.doublingHintFlag = doublingHintActive_ ? 1 : 0;
    out.quality.rrFallbackModeActive = rrFallbackModeActive_ ? 1 : 0;
    out.quality.pHalfOverFund = ratioHalfFund;
    out.quality.pairFrac = pairFrac;
    out.quality.rrShortFrac = shortFrac;
    out.quality.rrLongMs = longRR;
    out.quality.softStreak = softConsecPass_;
    out.quality.softSecs = softDoublingActive_ ? (lastTs_ - softStartTs_) : 0.0;
    // Logistic mapping for confidence (mirror active mapping used after updateSNR)
    double lastActiveTs3 = 0.0;
    if (softLastTrueTs_ > 0.0) lastActiveTs3 = std::max(lastActiveTs3, softLastTrueTs_);
    if (doublingLastTrueTs_ > 0.0) lastActiveTs3 = std::max(lastActiveTs3, doublingLastTrueTs_);
    if (hintLastTrueTs_ > 0.0) lastActiveTs3 = std::max(lastActiveTs3, hintLastTrueTs_);
    bool persistMap3 = (lastActiveTs3 > 0.0) && ((lastTs_ - lastActiveTs3) <= 5.0);
    bool activeConf3 = doublingHintActive_ || softDoublingActive_ || doublingActive_ || persistMap3;
    double x0 = activeConf3 ? 5.2 : 6.0; // center (dB)
    double k = activeConf3 ? (1.0/1.2) : 0.8;  // slope
    if (!std::isfinite(snrEmaDb_)) snrEmaDb_ = kSnrFallbackDb;
    double conf_snr = 1.0 / (1.0 + std::exp(-k * (snrEmaDb_ - x0)));
    if (!std::isfinite(conf_snr)) conf_snr = 0.0;
    // Multiply by (1 - rejection) and penalize high RR CV
    double conf = conf_snr * (1.0 - out.quality.rejectionRate);
    double cv = 0.0;
    if (!out.rrList.empty()) {
        double mean_rr = 0.0; for (double r : out.rrList) mean_rr += r; mean_rr /= (double)out.rrList.size();
        double var_rr = 0.0; for (double r : out.rrList) { double d = r - mean_rr; var_rr += d * d; }
        var_rr /= (double)out.rrList.size(); double sd_rr = std::sqrt(std::max(0.0, var_rr));
        cv = (mean_rr > 1e-9) ? (sd_rr / mean_rr) : 0.0;
        double kcv = activeConf3 ? 0.5 : 1.0;
        conf *= std::max(0.0, 1.0 - kcv * cv);
    }
    if (activeConf3) {
        double activeSecs = 0.0;
        if (softDoublingActive_) activeSecs = std::max(activeSecs, lastTs_ - softStartTs_);
        if (doublingHintActive_ && hintStartTs_ > 0.0) activeSecs = std::max(activeSecs, lastTs_ - hintStartTs_);
        if (out.quality.rejectionRate < 0.03 && cv < 0.12 && activeSecs >= 8.0) conf = std::min(1.0, conf * 1.1);
    }
    // Warm-up gate: require >=15s or >=15 beats before trusting confidence
    double warmupSecTarget = std::clamp(windowSec_ * 2.0, 4.0, 10.0);
    size_t warmupBeatsTarget = std::max<size_t>(4, static_cast<size_t>(std::ceil(windowSec_ * 1.5)));
    double elapsed = std::isfinite(warmupStartTs_) ? std::max(0.0, lastTs_ - warmupStartTs_) : std::max(0.0, lastTs_ - firstTsApprox_);
    double timeProgress = warmupSecTarget > 0.0 ? elapsed / warmupSecTarget : 1.0;
    size_t beatsInWindow = 0;
    if (!out.peakList.empty()) beatsInWindow = out.peakList.size();
    else if (!lastPeaks_.empty()) beatsInWindow = lastPeaks_.size();
    else if (!out.rrList.empty()) beatsInWindow = out.rrList.size() + 1;
    double beatProgress = (warmupBeatsTarget > 0)
        ? static_cast<double>(beatsInWindow) / static_cast<double>(warmupBeatsTarget)
        : 1.0;
    double warmProgress = std::clamp(std::max(timeProgress, beatProgress), 0.0, 1.0);
    conf *= warmProgress;
    if (!std::isfinite(conf)) conf = 0.0;
    out.quality.confidence = std::max(0.0, std::min(1.0, conf));
}

} // namespace heartpy
