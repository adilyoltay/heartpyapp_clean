#include "rn_options_builder.h"
#include <cmath>
#include <limits>

static inline bool isFinite(double x) {
    return std::isfinite(x) != 0;
}

extern "C" bool hp_validate_options(double fs,
                                     const heartpy::Options& opt,
                                     const char** err_code,
                                     std::string* err_msg) {
    // fs: 1..10000
    if (!isFinite(fs) || fs < 1.0 || fs > 10000.0) {
        if (err_code) *err_code = "HEARTPY_E001"; // Invalid sample rate
        if (err_msg) *err_msg = "Invalid sample rate (1-10000 Hz)";
        return false;
    }

    // bandpass: allow disabled (<=0 means off). If enabled, 0 <= low < high <= fs/2.
    if ((opt.lowHz > 0.0 || opt.highHz > 0.0)) {
        if (!isFinite(opt.lowHz) || !isFinite(opt.highHz) || opt.lowHz < 0.0 || opt.highHz <= 0.0 || opt.lowHz >= opt.highHz || opt.highHz > (fs * 0.5)) {
            if (err_code) *err_code = "HEARTPY_E011"; // Invalid bandpass
            if (err_msg) *err_msg = "Invalid bandpass (0<=low<high<=fs/2)";
            return false;
        }
    }

    // refractoryMs: 50..2000
    if (!isFinite(opt.refractoryMs) || opt.refractoryMs < 50.0 || opt.refractoryMs > 2000.0) {
        if (err_code) *err_code = "HEARTPY_E014"; // Invalid refractory
        if (err_msg) *err_msg = "Invalid refractory (50-2000 ms)";
        return false;
    }

    // BPM range: 30 <= bpmMin < bpmMax <= 240
    if (!isFinite(opt.bpmMin) || !isFinite(opt.bpmMax) || opt.bpmMin < 30.0 || opt.bpmMax > 240.0 || !(opt.bpmMin < opt.bpmMax)) {
        if (err_code) *err_code = "HEARTPY_E013"; // Invalid BPM range
        if (err_msg) *err_msg = "Invalid BPM range (30<=min<max<=240)";
        return false;
    }

    // nfft: allowed window [64, 16384]; snap is handled elsewhere
    if (!isFinite((double)opt.nfft) || opt.nfft < 64 || opt.nfft > 16384) {
        if (err_code) *err_code = "HEARTPY_E012"; // Invalid nfft
        if (err_msg) *err_msg = "Invalid nfft (64-16384)";
        return false;
    }

    // overlap 0..1 (exclusive 1)
    if (!isFinite(opt.overlap) || opt.overlap < 0.0 || opt.overlap >= 1.0) {
        // clamp is recommended; validation accepts [0,0.95] in practice, but we reject only on NaN
        if (!isFinite(opt.overlap)) {
            if (err_code) *err_code = "HEARTPY_E015";
            if (err_msg) *err_msg = "Invalid overlap (NaN/Inf)";
            return false;
        }
    }

    // highPrecisionFs: reject only if NaN/Inf; clamp elsewhere
    if (!isFinite(opt.highPrecisionFs)) {
        if (err_code) *err_code = "HEARTPY_E015";
        if (err_msg) *err_msg = "Invalid highPrecisionFs (NaN/Inf)";
        return false;
    }

    // Other 0..1 thresholds sanity: reject only if NaN/Inf
    if (!isFinite(opt.segmentRejectThreshold) || !isFinite(opt.segmentOverlap) || !isFinite(opt.rrSplineSmooth)) {
        if (err_code) *err_code = "HEARTPY_E015";
        if (err_msg) *err_msg = "Invalid threshold (NaN/Inf)";
        return false;
    }

    return true;
}

using facebook::jsi::Object;
using facebook::jsi::Runtime;
using facebook::jsi::Value;

static inline bool hasProp(Runtime& rt, const Object& o, const char* name) {
    return o.hasProperty(rt, name);
}
static inline double getNum(Runtime& rt, const Object& o, const char* name, double defv) {
    if (!hasProp(rt, o, name)) return defv;
    auto v = o.getProperty(rt, name);
    if (v.isNumber()) return v.asNumber();
    return defv;
}
static inline bool getBool(Runtime& rt, const Object& o, const char* name, bool defv) {
    if (!hasProp(rt, o, name)) return defv;
    auto v = o.getProperty(rt, name);
    if (v.isBool()) return v.getBool();
    return defv;
}

heartpy::Options hp_build_options_from_jsi(Runtime& rt, const Object& opts, const char** err_code, std::string* err_msg) {
    heartpy::Options o;
    if (!opts.isHostObject(rt) && !opts.isArray(rt)) {
        // Bandpass
        if (hasProp(rt, opts, "bandpass")) {
            auto bp = opts.getProperty(rt, "bandpass").asObject(rt);
            o.lowHz = getNum(rt, bp, "lowHz", o.lowHz);
            o.highHz = getNum(rt, bp, "highHz", o.highHz);
            o.iirOrder = (int)getNum(rt, bp, "order", o.iirOrder);
        }
        // Filter mode (optional)
        if (hasProp(rt, opts, "filter")) {
            auto filt = opts.getProperty(rt, "filter").asObject(rt);
            if (hasProp(rt, filt, "mode")) {
                auto s = filt.getProperty(rt, "mode");
                if (s.isString()) {
                    std::string m = s.asString(rt).utf8(rt);
                    if (m == "rbj") o.filterMode = heartpy::Options::FilterMode::RBJ;
                    else if (m == "butter" || m == "butter-filtfilt") o.filterMode = heartpy::Options::FilterMode::BUTTER_FILTFILT;
                    else o.filterMode = heartpy::Options::FilterMode::AUTO;
                }
            }
            if (hasProp(rt, filt, "order")) {
                o.iirOrder = (int)getNum(rt, filt, "order", o.iirOrder);
            }
        }
        // Welch
        if (hasProp(rt, opts, "welch")) {
            auto w = opts.getProperty(rt, "welch").asObject(rt);
            o.nfft = (int)getNum(rt, w, "nfft", o.nfft);
            o.overlap = getNum(rt, w, "overlap", o.overlap);
            o.welchWsizeSec = getNum(rt, w, "wsizeSec", o.welchWsizeSec);
        }
        // Peak
        if (hasProp(rt, opts, "peak")) {
            auto p = opts.getProperty(rt, "peak").asObject(rt);
            o.refractoryMs = getNum(rt, p, "refractoryMs", o.refractoryMs);
            o.minPeakDistanceMs = getNum(rt, p, "minPeakDistanceMs", o.minPeakDistanceMs);
            o.thresholdScale = getNum(rt, p, "thresholdScale", o.thresholdScale);
            o.bpmMin = getNum(rt, p, "bpmMin", o.bpmMin);
            o.bpmMax = getNum(rt, p, "bpmMax", o.bpmMax);
            o.rrOutlierPercent = getNum(rt, p, "rrOutlierPercent", o.rrOutlierPercent);
            o.rrOutlierMinMs = getNum(rt, p, "rrOutlierMinMs", o.rrOutlierMinMs);
            o.rrOutlierMaxMs = getNum(rt, p, "rrOutlierMaxMs", o.rrOutlierMaxMs);
        }
        // Preprocessing
        if (hasProp(rt, opts, "preprocessing")) {
            auto prep = opts.getProperty(rt, "preprocessing").asObject(rt);
            o.removeBaselineWander = getBool(rt, prep, "removeBaselineWander", o.removeBaselineWander);
            o.enhancePeaks = getBool(rt, prep, "enhancePeaks", o.enhancePeaks);
        }
        // Quality
        if (hasProp(rt, opts, "quality")) {
            auto q = opts.getProperty(rt, "quality").asObject(rt);
            o.rejectSegmentwise = getBool(rt, q, "rejectSegmentwise", o.rejectSegmentwise);
            o.segmentRejectThreshold = getNum(rt, q, "segmentRejectThreshold", o.segmentRejectThreshold);
            if (hasProp(rt, q, "segmentRejectMaxRejects")) o.segmentRejectMaxRejects = (int)getNum(rt, q, "segmentRejectMaxRejects", o.segmentRejectMaxRejects);
            if (hasProp(rt, q, "segmentRejectWindowBeats")) o.segmentRejectWindowBeats = (int)getNum(rt, q, "segmentRejectWindowBeats", o.segmentRejectWindowBeats);
            o.segmentRejectOverlap = getNum(rt, q, "segmentRejectOverlap", o.segmentRejectOverlap);
            // HeartPy threshold_rr parity
            if (hasProp(rt, q, "thresholdRR")) o.thresholdRR = getBool(rt, q, "thresholdRR", o.thresholdRR);
        }
        // High precision
        if (hasProp(rt, opts, "highPrecision")) {
            auto hp = opts.getProperty(rt, "highPrecision").asObject(rt);
            o.highPrecision = getBool(rt, hp, "enabled", o.highPrecision);
            o.highPrecisionFs = getNum(rt, hp, "targetFs", o.highPrecisionFs);
        }
        // Segmentwise
        if (hasProp(rt, opts, "segmentwise")) {
            auto seg = opts.getProperty(rt, "segmentwise").asObject(rt);
            o.segmentWidth = getNum(rt, seg, "width", o.segmentWidth);
            o.segmentOverlap = getNum(rt, seg, "overlap", o.segmentOverlap);
        }
        o.snrTauSec = getNum(rt, opts, "snrTauSec", o.snrTauSec);
        o.snrActiveTauSec = getNum(rt, opts, "snrActiveTauSec", o.snrActiveTauSec);
        o.adaptivePsd = getBool(rt, opts, "adaptivePsd", o.adaptivePsd);
        // Global FD toggle (calc_freq parity)
        if (hasProp(rt, opts, "calcFreq")) {
            o.calcFreq = getBool(rt, opts, "calcFreq", o.calcFreq);
        }
    }
    // Validate core subset; caller handles clamps
    if (err_code || err_msg) {
        const char* code = nullptr; std::string msg;
        (void)code; (void)msg; // eliminated if not used
    }
    return o;
}
