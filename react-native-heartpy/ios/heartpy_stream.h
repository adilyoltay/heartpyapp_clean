// Realtime streaming analyzer (skeleton) — Phase S1
#pragma once

#include <vector>
#include <deque>
#include <cstddef>
#include <mutex>
#include <algorithm>
#include <limits>
#include "heartpy_core.h"

namespace heartpy {

// Simple fixed-capacity ring buffer for POD types
template <typename T>
class RingBuffer {
public:
    RingBuffer() = default;
    explicit RingBuffer(size_t cap) { reconfigure(cap); }
    void reconfigure(size_t cap) {
        if (cap == 0) cap = 1;
        std::vector<T> nb(cap);
        // copy last min(size, cap) elements into new buffer
        size_t keep = std::min(size_, cap);
        for (size_t i = 0; i < keep; ++i) {
            nb[keep - 1 - i] = at(size_ - 1 - i);
        }
        buf_.swap(nb);
        cap_ = cap;
        head_ = 0;
        size_ = keep;
    }
    size_t capacity() const { return cap_; }
    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    // Push single value
    inline void push_back(const T& v) {
        if (cap_ == 0) reconfigure(1);
        if (size_ < cap_) {
            buf_[(head_ + size_) % cap_] = v;
            ++size_;
        } else {
            // overwrite oldest
            buf_[head_] = v;
            head_ = (head_ + 1) % cap_;
        }
    }
    // Push many values
    void push_back_many(const T* data, size_t n) {
        if (!data || n == 0) return;
        for (size_t i = 0; i < n; ++i) push_back(data[i]);
    }
    // Access i-th element from oldest (0..size-1)
    inline const T& at(size_t i) const {
        return buf_[(head_ + i) % cap_];
    }
    // Snapshot into contiguous vector (oldest..newest)
    void snapshot(std::vector<T>& out) const {
        out.resize(size_);
        if (size_ == 0) return;
        size_t first = head_;
        size_t n1 = std::min(size_, cap_ - first);
        for (size_t i = 0; i < n1; ++i) out[i] = buf_[first + i];
        for (size_t i = n1; i < size_; ++i) out[i] = buf_[i - n1];
    }
private:
    std::vector<T> buf_;
    size_t cap_{0};
    size_t head_{0};
    size_t size_{0};
};

struct SBiquad {
    double b0{0}, b1{0}, b2{0}, a1{0}, a2{0};
    double z1{0}, z2{0};
    inline float process(float in) {
        double out = in * b0 + z1;
        z1 = in * b1 + z2 - a1 * out;
        z2 = in * b2 - a2 * out;
        return static_cast<float>(out);
    }
};

// Double-precision biquad for high-precision/deterministic mode
struct SBiquadD {
    double b0{0}, b1{0}, b2{0}, a1{0}, a2{0};
    double z1{0}, z2{0};
    inline double process(double in) {
        double out = in * b0 + z1;
        z1 = in * b1 + z2 - a1 * out;
        z2 = in * b2 - a2 * out;
        return out;
    }
};

// A minimal, non-breaking streaming API skeleton.
// Internally uses a batch fallback on the sliding window until
// fully incremental path (peaks/filters) is implemented in later phases.
class RealtimeAnalyzer {
public:
    explicit RealtimeAnalyzer(double fs, const Options& opt = {});

    void setWindowSeconds(double sec);              // 10–60 seconds typical
    void setUpdateIntervalSeconds(double sec);      // default 1.0 second
    void setPsdUpdateSeconds(double sec) { std::lock_guard<std::mutex> lock(dataMutex_); psdUpdateSec_ = std::clamp(sec, 0.5, 5.0); }
    void setDisplayHz(double hz) { std::lock_guard<std::mutex> lock(dataMutex_); displayHz_ = std::clamp(hz, 10.0, 120.0); }
    // Convenience presets (may adjust filter/threshold defaults)
    void applyPresetTorch() { opt_.lowHz = 0.7; opt_.highHz = 3.0; opt_.refractoryMs = std::max(300.0, opt_.refractoryMs); opt_.useHPThreshold = true; opt_.maPerc = std::max(10.0, std::min(60.0, opt_.maPerc)); }
    void applyPresetAmbient() { opt_.lowHz = 0.5; opt_.highHz = 3.5; opt_.thresholdScale = std::max(0.5, opt_.thresholdScale); opt_.refractoryMs = std::max(320.0, opt_.refractoryMs); opt_.useHPThreshold = true; opt_.maPerc = std::max(10.0, std::min(60.0, opt_.maPerc)); }

    void push(const float* samples, size_t n, double t0 = 0.0);
    void push(const std::vector<double>& samples, double t0 = 0.0);
    // Optional: per-sample timestamps in seconds for variable-fps sources
    void push(const float* samples, const double* timestamps, size_t n);

    // If a new update is ready (>= update interval), fills out and returns true
    bool poll(HeartMetrics& out);

    QualityInfo getQuality() const { std::lock_guard<std::mutex> lock(dataMutex_); return lastQuality_; }
    std::vector<int> latestPeaks() const { std::lock_guard<std::mutex> lock(dataMutex_); return lastPeaks_; }
    std::vector<double> latestRR() const { std::lock_guard<std::mutex> lock(dataMutex_); return lastRR_; }
    std::vector<float> displayBuffer() const { std::lock_guard<std::mutex> lock(dataMutex_); return displayBuf_; }

private:
    void append(const float* x, size_t n);
    void trimToWindow();
    void updateSNR(HeartMetrics& out);
    // Thread safety
    mutable std::mutex dataMutex_;

    // Performance scratch buffers (reused to avoid frequent reallocations)
    double medianOfRR(const std::vector<double>& rr);
    std::vector<double> scratchRR_;
    std::vector<double> yBufferD_;
    std::vector<double> noiseScratch_;
    std::vector<char> keepScratch_;
    std::vector<double> lastPsdFreq_;
    std::vector<double> lastPsdPower_;

    double fs_ {0.0};              // nominal fs from constructor
    Options opt_ {};
    double windowSec_ {60.0};
    double updateSec_ {1.0};

    // timebase (seconds)
    double lastEmitTime_ {0.0};              // last poll emit time in seconds
    double lastTs_ {0.0};                    // last appended sample timestamp (sec)
    double firstTsApprox_ {0.0};             // approx timestamp of first sample in window (sec)
    double warmupStartTs_ {std::numeric_limits<double>::quiet_NaN()}; // true stream start timestamp
    double effectiveFs_ {0.0};               // EMA-smoothed fs if timestamps are provided
    double emaAlpha_ {0.1};                  // smoothing for effective Fs
    double lastPsdTime_ {0.0};               // last PSD update time (sec)
    double psdUpdateSec_ {2.0};              // compute PSD/SNR every ~2s
    double displayHz_ {60.0};                // downsampled display rate (Hz)

    // Sliding window buffers (raw for now; later phases will hold filtered/causal)
    std::vector<double> m_signal_buffer;
    std::vector<double> m_timestamps;
    std::vector<float> filt_;
    std::vector<float> displayBuf_; // downsampled view for UI
    std::vector<double> pollWindowBuffer_;
    std::vector<double> pollTimestampBuffer_;
    std::vector<SBiquad> bq_;
    std::vector<SBiquadD> bqD_;
    // Optional ring storage (when opt_.useRingBuffer == true)
    bool useRing_ {false};
    RingBuffer<float> ringSignal_;
    RingBuffer<float> ringFilt_;
    size_t ringCapacity_ {0};

    // Cached outputs from last poll
    QualityInfo lastQuality_ {};
    std::vector<int> lastPeaks_ {};
    std::vector<double> lastRR_ {};

    // Rolling stats for thresholding
    std::deque<float> rollWin_;
    double rollSum_ {0.0};
    double rollSumSq_ {0.0};
    // Rectified window (for thresholding only)
    std::deque<float> rollWinRect_;
    double rollRectSum_ {0.0};
    double rollRectSumSq_ {0.0};
    // Monotonic deques for O(1) min/max over rectified window
    std::deque<float> rectMinQ_;
    std::deque<float> rectMaxQ_;
    int winSamples_ {0};
    int refractorySamples_ {0};
    size_t firstAbs_ {0};
    size_t totalAbs_ {0};
    std::vector<size_t> peaksAbs_;
    size_t acceptedPeaksTotal_ {0};

    // Audit/telemetry counters
    unsigned long long droppedSamplesTotal_ {0};
    unsigned long long clampedBatchesTotal_ {0};
    unsigned long long oomPreventedTotal_ {0};
    unsigned long long paramChangeEventsTotal_ {0};
    int lastMergeBudgetExhausted_ {0};
    unsigned long long mergeBudgetExhaustedTotal_ {0};
    unsigned long long droppedSamplesLast_ {0};
    unsigned long long clampedBatchesLast_ {0};
    int dropConsecPolls_ {0};
    unsigned long long timestampBacktrackEventsTotal_ {0};
    unsigned long long timestampsSkippedTotal_ {0};
    unsigned long long timeJumpEventsTotal_ {0};
    unsigned long long psdParamClampEventsTotal_ {0};
    unsigned long long psdReuseFallbackEventsTotal_ {0};
    unsigned long long psdTimeDomainFallbackEventsTotal_ {0};
    unsigned long long psdInvalidFramesTotal_ {0};

#ifdef HEARTPY_LOCK_TIMING
public:
    // which: 1 = snapshot lock, 2 = commit lock
    static void lockStatsGet(int which, double& avg_us, double& p95_us, bool reset);
    static void recordLockHold(int which, double us);
#endif

    // HP-style thresholding state
    double baseLift_ {0.0};         // mn = mean(rolling_mean)/100 * maPerc_
    double maPerc_ {30.0};          // current ma_perc selection
    bool   hpThreshold_ {false};    // whether streaming uses HP-style threshold

    // Adaptive ma_perc tuning cadence and hysteresis
    double lastMaUpdateTime_ {0.0};
    double lastMaChangeTime_ {0.0};
    double maUpdateSec_ {3.0};
    double maPercScore_ {1e300}; // lower RR SD score is better

    // SNR smoothing (EMA)
    double snrEmaDb_ {0.0};
    bool   snrEmaValid_ {false};
    double snrTauSec_ {10.0};
    double lastSnrUpdateTime_ {0.0};
    bool   lastSnrActiveMode_ {false};
    double lastSnrBaseBw_ {0.12};

    // Streaming BPM prior (for ma_perc bias)
    double bpmEma_ {0.0};
    bool   bpmEmaValid_ {false};
    double bpmTauSec_ {8.0};
    double lastBpmUpdateTime_ {0.0};

    // Last valid HR frequency used for SNR
    double lastF0Hz_ {0.0};
    double lastRefMsActive_ {0.0};
    double lastMinRRBoundMs_ {0.0};
    bool   warmupWasPassed_ {false};
    double hardFallbackUntil_ {0.0};

    // RR-gating state
    int    shortRejectCount_ {0};
    double shortRejectWindowStart_ {0.0};
    double tempLiftBoost_ {0.0};          // temporary extra lift in 0..1024 units
    double tempLiftUntil_ {0.0};          // time until which temp lift applies
    int    dynRefExtraSamples_ {0};       // temporary extra refractory samples
    double dynRefUntil_ {0.0};
    double lastAcceptedAmpCmp_ {0.0};     // last accepted peak amplitude in comparison scale

    // Persistent high-HR & CV tracking for ma_perc floors
    double cvHighStartTs_ {0.0};
    bool   cvHighActive_ {false};

    // Persistent high-HR tracking for grid bias
    double bpmHighStartTs_ {0.0};
    bool   bpmHighActive_ {false};

    // Harmonic suppression state
    bool   softDoublingActive_ {false};
    int    softConsecPass_ {0};
    double softStartTs_ {0.0};
    double softLastTrueTs_ {0.0};
    std::deque<double> halfF0Hist_;
    bool   doublingActive_ {false};
    double doublingLastTrueTs_ {0.0};
    double doublingHoldUntil_ {0.0};
    double doublingLongRRms_ {0.0};
    // Violation persistence tracking for auto-clear of soft/hard
    double lastClearBadStart_ {0.0};
    // PSD-only doubling hint (post warm-up) to unlock doubles conservatively
    bool   doublingHintActive_ {false};
    double hintLastTrueTs_ {0.0};
    double hintStartTs_ {0.0};
    double hintHoldUntil_ {0.0};
    double lastHintBadStart_ {0.0};
    // Temporary relaxation when oversuppression detected
    double chokeRelaxUntil_ {0.0};
    double chokeStartTs_ {0.0};

    bool   lastPsdValid_ {false};
    double lastPsdFs_ {0.0};
    int    lastPsdNfft_ {0};
    double lastPsdOverlap_ {0.0};
    // RR-based fallback tracking
    int    rrFallbackConsec_ {0};
    bool   rrFallbackActive_ {false};
    bool   rrFallbackDrivingHint_ {false};
    double lastPollBpmEst_ {0.0};
    bool   rrFallbackModeActive_ {false};
};

} // namespace heartpy

// Optional plain C bridge (symbols have C linkage; still compiled as C++)
extern "C" {
    void* hp_rt_create(double fs, const heartpy::Options* opt);
    void  hp_rt_set_window(void* h, double sec);
    void  hp_rt_set_update_interval(void* h, double sec);
    void  hp_rt_push(void* h, const float* x, size_t n, double t0);
    // Per-sample timestamped push (seconds)
    void  hp_rt_push_ts(void* h, const float* x, const double* ts, size_t n);
    int   hp_rt_poll(void* h, heartpy::HeartMetrics* out);
    void  hp_rt_destroy(void* h);
}
