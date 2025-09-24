#pragma once

#include <vector>
#include <functional>

#ifdef USE_KISSFFT
#include "kiss_fftr.h"
#endif

namespace heartpy {

// Enhanced Options structure with all Python HeartPy features
struct Options {
	enum class FilterMode { AUTO, RBJ, BUTTER_FILTFILT };
	// Bandpass filtering
	double lowHz = 0.5;
	double highHz = 5.0;
	int iirOrder = 2;
	FilterMode filterMode = FilterMode::AUTO; // AUTO: legacy; RBJ or BUTTER_FILTFILT selectable

	// Welch PSD
	int nfft = 256;              // used if explicitly set; otherwise derived from welchWsizeSec
	double overlap = 0.5;        // ratio 0..1 (50% default like SciPy)
	double welchWsizeSec = 240;  // HeartPy default Welch window size in seconds
    bool adaptivePsd = true;    // enable dynamic Welch parameter tuning/fallbacks
    // RR spline smoothing controls
    double rrSplineSmooth = 0.1; // legacy: blend factor 0..1 for pre-smoothing
    double rrSplineS = 10.0;      // UnivariateSpline-like smoothing factor (quick test default ~10)
    double rrSplineSTargetSse = 0.0; // If >0, target sum of squared residuals for Reinsch smoothing (bisection to find lambda)

	// Segmentwise rejection (check_binary_quality)
    int segmentRejectMaxRejects = 3; // maximum rejects per 10-beat window

    // Breathing output control
    bool breathingAsBpm = false; // false: Hz (HeartPy), true: breaths/min
    // Frequency-domain computation control (parity with HeartPy calc_freq)
    bool calcFreq = true;       // if false, skip VLF/LF/HF and LF/HF computation

	// Peak detection
	double refractoryMs = 150.0; // P1 FIX: Reduced from 250ms to 150ms for better sensitivity
	double minPeakDistanceMs = 320.0; // Post-processing guard to drop tightly spaced beats
	double thresholdScale = 0.3; // P1 FIX: Reduced from 0.5 to 0.3 for more sensitive peak detection
	double bpmMin = 35.0; // P1 FIX: Lowered from 40 to 35 for broader range
	double bpmMax = 180.0;
	double rrOutlierPercent = 0.25;  // tighten RR outlier band (was 30%)
	double rrOutlierMinMs = 180.0;   // enforce minimum absolute band
	double rrOutlierMaxMs = 320.0;   // cap RR band expansion

	// HP-style thresholding (rolling_mean + ma_perc lift)
	bool   useHPThreshold = false;   // if true, prefer HP-style threshold in streaming
	double maPerc = 30.0;            // HeartPy-like ma_perc (10..60 typical)
	bool   adaptiveMaPerc = true;    // enable light grid search per poll

	// Streaming tunables (defaults preserve current behavior)
	// Min-RR gating
	double minRRGateFactor = 0.86;     // multiply longRR estimate
	double minRRFloorRelaxed = 400.0;  // ms floor after warmup
	double minRRFloorStrict = 500.0;   // ms floor during early phase
	double minRRCeiling = 1200.0;      // ms ceiling for RR bounds

	// Periodic suppression tolerance (fraction of period)
	double periodicSuppressionTol = 0.24;

	// RR merge bands
	double rrMergeBandLow = 0.75;      // lower bound for near-median checks
	double rrMergeBandHigh = 1.25;     // upper bound for near-median checks
	double rrMergeEqualBandLow = 0.85; // equal-pair near-median band low
	double rrMergeEqualBandHigh = 1.15;// equal-pair near-median band high

	// PSD half/fund ratio thresholds
	double pHalfOverFundThresholdSoft = 2.0; // soft activation
	double pHalfOverFundThresholdLow = 1.6;  // looser hint hold

    // SNR band and EMA behavior (OPTIMIZED for better responsiveness)
    double snrBandPassive = 0.15;      // Hz half-width in passive mode (+25% wider)
    double snrBandActive = 0.25;       // Hz half-width in active mode (+39% wider)
    double snrActiveTauSec = 2.0;      // EMA tau when active (-71% faster response)
    double snrTauSec = 3.0;            // EMA tau in passive mode (-70% faster response)
    double snrBandBlendFactor = 0.30;  // blend toward instant when band changes

    // PSD stability options (defaults keep current behavior)
    int    halfF0HistLen = 5;          // history length for half-f0 stability
    double halfF0TolHzWarm = 0.06;     // drift tol after warm-up
    double halfF0TolHzCold = 0.10;     // drift tol before warm-up

    // Choke recovery options
    double chokeRelaxBaseSec = 5.0;    // recovery when bpm >= threshold
    double chokeRelaxLowBpmSec = 7.0;  // recovery when bpm < threshold
    double chokeBpmThreshold = 35.0;   // bpm threshold for low-BPM recovery

	// Preprocessing options
	bool interpClipping = false;
	double clippingThreshold = 1020.0;
	bool hampelCorrect = false;
	int hampelWindow = 6;
	double hampelThreshold = 3.0;
	bool removeBaselineWander = false;
	bool enhancePeaks = false;

	// High precision mode
	bool highPrecision = false;
	double highPrecisionFs = 1000.0;

    // Quality assessment
    bool rejectSegmentwise = false;
    double segmentRejectThreshold = 0.3; // reject if >30% bad beats
    int segmentRejectWindowBeats = 10;   // window size for binary quality check
    double segmentRejectOverlap = 0.0;   // 0..1 overlap ratio between successive windows

	// RR cleaning
	bool cleanRR = false;
	enum class CleanMethod { QUOTIENT_FILTER, IQR, Z_SCORE } cleanMethod = CleanMethod::QUOTIENT_FILTER;
    int cleanIterations = 2; // iterations for quotient filter (HeartPy default ~2)

    // RR thresholding (HeartPy threshold_rr): default false
    bool thresholdRR = false;

	// SDSD computation mode (signed vs abs diffs)
	enum class SdsdMode { SIGNED, ABS } sdsdMode = SdsdMode::ABS;

	// Poincaré SD1/SD2 computation mode (formula vs masked pairs)
	enum class PoincareMode { FORMULA, MASKED } poincareMode = PoincareMode::MASKED;

	// pNN output as percent (0..100) or ratio (0..1)
	bool pnnAsPercent = true;

    // Segmentwise analysis
    double segmentWidth = 120.0; // seconds
    double segmentOverlap = 0.0; // 0..1
    double segmentMinSize = 20.0; // seconds
    bool replaceOutliers = false;

    // Streaming storage (optional)
    bool useRingBuffer = false; // if true, use fixed-capacity ring buffers in streaming (default OFF)
    
    // Deterministic mode (runtime): prefer scalar/DFT paths, snap EMA cadence
    bool deterministic = false; // default OFF
};

// Quality information structure
struct QualityInfo {
	int totalBeats = 0;
	int rejectedBeats = 0;
	double rejectionRate = 0.0;
	std::vector<int> rejectedIndices;
	bool goodQuality = true;
	std::string qualityWarning;
    // Streaming additions (optional fields)
    double snrDb = 0.0;        // estimated SNR in dB (0 if unavailable)
    double confidence = 0.0;   // 0..1 confidence score (0 if unavailable)
    double f0Hz = 0.0;         // estimated HR fundamental frequency used for SNR (may be harmonically adjusted)
    double maPercActive = 0.0; // active ma_perc (HP threshold) if applicable
    // Harmonic suppression diagnostics
    int    doublingFlag = 0;   // 1 if harmonic suppression active, else 0
    int    softDoublingFlag = 0; // 1 if PSD-only soft flag active
    double rrShortFrac = 0.0;  // fraction of short RR cluster
    double rrLongMs = 0.0;     // long RR (ms) used when suppression active
    double pHalfOverFund = 0.0; // PSD ratio P(1/2 f0)/P(f0)
    double pairFrac = 0.0;      // fraction of adjacent pairs summing to ~longRR
    // Acceptance diagnostics
    double refractoryMsActive = 0.0; // current refractory applied (ms)
    double minRRBoundMs = 0.0;       // current min RR bound applied (ms)
    int    softStreak = 0;           // consecutive PSD updates passing soft criteria
    double softSecs = 0.0;           // seconds since soft flag became active
    int    hardFallbackActive = 0;   // 1 when hard fallback elevated refractory is active
    int    doublingHintFlag = 0;     // 1 when PSD-only doubling hint is active
    int    rrFallbackModeActive = 0; // 1 when RR-only fallback mode gating is active (debug)
    int    snrWarmupActive = 0;      // 1 when SNR warm-up guard is active
    double snrSampleCount = 0.0;     // samples available to SNR computation

    // Audit/telemetry (non-blocking; full JSON only)
    unsigned long long droppedSamplesTotal = 0;      // cumulative samples dropped (vector path trimming)
    unsigned long long clampedBatchesTotal = 0;      // times input batches were clamped
    unsigned long long oomPreventedTotal = 0;        // times capacity was saturated via safe sizing
    unsigned long long paramChangeEventsTotal = 0;   // parameter change events (window/preset/update interval)
    int mergeBudgetExhausted = 0;                    // 1 if this poll hit merge budget cap
    unsigned long long mergeBudgetExhaustedTotal = 0;// cumulative polls hitting merge cap
    // Per-poll deltas and timestamp events
    unsigned long long droppedSamplesLast = 0;       // samples dropped in this poll's trimming
    unsigned long long clampedBatchesLast = 0;       // batches clamped in this poll
    unsigned long long timestampBacktrackEventsTotal = 0; // non-monotonic timestamp events (cumulative)
    unsigned long long timestampsSkippedTotal = 0;   // timestamps skipped due to backtrack (cumulative)
    unsigned long long timeJumpEventsTotal = 0;      // dt > threshold events (cumulative)
    int droppingActive = 0;                          // 1 if consecutive drops observed recently
};

// Enhanced metrics structure matching Python HeartPy
struct HeartMetrics {
	// Basic metrics
	double bpm = 0.0;
	std::vector<double> ibiMs; // inter-beat intervals in ms
	std::vector<double> peakTimestamps; // timestamps of detected peaks
	std::vector<double> rrList; // clean RR intervals
    std::vector<int> peakList; // peak indices
    std::vector<int> peakListRaw; // pre-cleaning peaks
    std::vector<int> binaryPeakMask; // 1=accepted, 0=rejected (aligned to peakListRaw)

	// Snapshot waveforms (synchronized with current analysis window)
	std::vector<double> waveform_values;
	std::vector<double> waveform_timestamps;

	// Time domain measures
	double sdnn = 0.0;
	double rmssd = 0.0;
	double sdsd = 0.0;
	double pnn20 = 0.0;
	double pnn50 = 0.0;
	double nn20 = 0.0; // absolute count
	double nn50 = 0.0; // absolute count
	double mad = 0.0;   // median absolute deviation

	// Poincaré analysis
	double sd1 = 0.0;
	double sd2 = 0.0;
	double sd1sd2Ratio = 0.0;
	double ellipseArea = 0.0;

	// Frequency domain (Welch)
	double vlf = 0.0;
	double lf = 0.0;
	double hf = 0.0;
	double lfhf = 0.0;
	double totalPower = 0.0;
	double lfNorm = 0.0;  // normalized LF
	double hfNorm = 0.0;  // normalized HF

	// Breathing analysis
	double breathingRate = 0.0;

	// Quality metrics
	QualityInfo quality;

    // Segmentwise results (if applicable)
    std::vector<HeartMetrics> segments;

    // Binary quality segments (10-beat windows by default)
    struct BinarySegment {
        int index = 0;          // segment ordinal
        int startBeat = 0;      // start index in peakListRaw
        int endBeat = 0;        // end index (exclusive)
        int totalBeats = 0;     // beats in segment
        int rejectedBeats = 0;  // number rejected
        bool accepted = true;   // whether segment passes threshold
    };
    std::vector<BinarySegment> binarySegments;
};

// Main API functions matching Python HeartPy interface

// Primary analysis function (equivalent to hp.process)
HeartMetrics analyzeSignal(const std::vector<double>& signal, double fs, const Options& opt = {});

// Segmentwise analysis (equivalent to hp.process_segmentwise)
HeartMetrics analyzeSignalSegmentwise(const std::vector<double>& signal, double fs, const Options& opt = {});

// RR-only analysis (equivalent to hp.process_rr)
HeartMetrics analyzeRRIntervals(const std::vector<double>& rrMs, const Options& opt = {});

// Preprocessing functions
std::vector<double> interpolateClipping(const std::vector<double>& signal, double fs, double threshold = 1020.0);
std::vector<double> hampelFilter(const std::vector<double>& signal, int windowSize = 6, double threshold = 3.0);
std::vector<double> removeBaselineWander(const std::vector<double>& signal, double fs);
std::vector<double> enhancePeaks(const std::vector<double>& signal, double fs);
std::vector<double> scaleData(const std::vector<double>& signal, double newMin = 0.0, double newMax = 1024.0);

// Outlier detection functions
std::vector<double> removeOutliersIQR(const std::vector<double>& data, double& lowerBound, double& upperBound);
std::vector<double> removeOutliersZScore(const std::vector<double>& data, double threshold = 3.0);
std::vector<double> removeOutliersQuotientFilter(const std::vector<double>& rrIntervals);

// Quality assessment
QualityInfo assessSignalQuality(const std::vector<double>& signal, const std::vector<int>& peaks, double fs);
bool checkSegmentQuality(const std::vector<int>& rejectedBeats, int totalBeats, double threshold = 0.3);

// Breathing analysis
double calculateBreathingRate(const std::vector<double>& rrIntervals, const std::string& method = "welch");

// High precision peak detection
std::vector<int> interpolatePeaks(const std::vector<double>& signal, const std::vector<int>& peaks, 
                                  double originalFs, double targetFs);

// Utility functions
double calculateMAD(const std::vector<double>& data); // Median Absolute Deviation
std::vector<double> calculatePoincare(const std::vector<double>& rrIntervals);
std::pair<std::vector<double>, std::vector<double>> welchPowerSpectrum(const std::vector<double>& signal, 
                                                                        double fs, int nfft = 256, double overlap = 0.5);

// Diagnostics for PSD guard fallbacks
unsigned long long getWelchPsdGuardFallbackCount();
unsigned long long getWelchPsdGuardFailureCount();

// Global deterministic toggle for core spectral routines (runtime)
void setDeterministic(bool on);
bool isDeterministic();

}
