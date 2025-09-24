#include <jni.h>
#include <vector>
#include <algorithm>
#include <sstream>
#include <string>
#include <android/log.h>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <jsi/jsi.h>
#include "../../../../cpp/heartpy_core.h"
// Realtime streaming API
#include "../../../../cpp/heartpy_stream.h"
// RN options validator (step 1)
#include "../../cpp/rn_options_builder.h"

static std::string to_json(const heartpy::HeartMetrics& r, bool includeSegments=false) {
    std::ostringstream os;
    os << "{";
    auto arr = [&](const char* k, const std::vector<double>& v){
        os << "\"" << k << "\":" << "[";
        for (size_t i=0;i<v.size();++i){ if(i) os << ","; os << v[i]; }
        os << "]";
    };
    auto arr_i = [&](const char* k, const std::vector<int>& v){
        os << "\"" << k << "\":" << "[";
        for (size_t i=0;i<v.size();++i){ if(i) os << ","; os << v[i]; }
        os << "]";
    };
    auto kv = [&](const char* k, double v){ os << "\""<<k<<"\":"<<v; };
    // scalars
    kv("bpm", r.bpm); os << ",";
    kv("sdnn", r.sdnn); os << ","; kv("rmssd", r.rmssd); os << ","; kv("sdsd", r.sdsd); os << ",";
    kv("pnn20", r.pnn20); os << ","; kv("pnn50", r.pnn50); os << ","; kv("nn20", r.nn20); os << ","; kv("nn50", r.nn50); os << ","; kv("mad", r.mad); os << ",";
    kv("sd1", r.sd1); os << ","; kv("sd2", r.sd2); os << ","; kv("sd1sd2Ratio", r.sd1sd2Ratio); os << ","; kv("ellipseArea", r.ellipseArea); os << ",";
    kv("vlf", r.vlf); os << ","; kv("lf", r.lf); os << ","; kv("hf", r.hf); os << ","; kv("lfhf", r.lfhf); os << ","; kv("totalPower", r.totalPower); os << ","; kv("lfNorm", r.lfNorm); os << ","; kv("hfNorm", r.hfNorm); os << ",";
    kv("breathingRate", r.breathingRate); os << ",";
    // arrays
    arr("ibiMs", r.ibiMs); os << ","; arr("rrList", r.rrList); os << ","; arr_i("peakList", r.peakList); os << ",";
    // timestamps of detected peaks (if available)
    arr("peakTimestamps", r.peakTimestamps); os << ",";
    arr("waveform_values", r.waveform_values); os << ",";
    arr("waveform_timestamps", r.waveform_timestamps); os << ",";
    arr_i("peakListRaw", r.peakListRaw); os << ",";
    arr_i("binaryPeakMask", r.binaryPeakMask); os << ",";
    // quality
    os << "\"quality\":{";
    kv("totalBeats", r.quality.totalBeats); os << ","; kv("rejectedBeats", r.quality.rejectedBeats); os << ","; kv("rejectionRate", r.quality.rejectionRate); os << ",";
    os << "\"goodQuality\":" << (r.quality.goodQuality ? "true" : "false");
    // Streaming metrics (if available)
    os << ",\"snrDb\":" << r.quality.snrDb;
    os << ",\"confidence\":" << r.quality.confidence;
    os << ",\"f0Hz\":" << r.quality.f0Hz;
    os << ",\"maPercActive\":" << r.quality.maPercActive;
    os << ",\"doublingFlag\":" << r.quality.doublingFlag;
    os << ",\"softDoublingFlag\":" << r.quality.softDoublingFlag;
    os << ",\"doublingHintFlag\":" << r.quality.doublingHintFlag;
    os << ",\"hardFallbackActive\":" << r.quality.hardFallbackActive;
    os << ",\"rrFallbackModeActive\":" << r.quality.rrFallbackModeActive;
    os << ",\"snrWarmupActive\":" << r.quality.snrWarmupActive;
    os << ",\"snrSampleCount\":" << r.quality.snrSampleCount;
    os << ",\"refractoryMsActive\":" << r.quality.refractoryMsActive;
    os << ",\"minRRBoundMs\":" << r.quality.minRRBoundMs;
    os << ",\"pairFrac\":" << r.quality.pairFrac;
    os << ",\"rrShortFrac\":" << r.quality.rrShortFrac;
    os << ",\"rrLongMs\":" << r.quality.rrLongMs;
    os << ",\"pHalfOverFund\":" << r.quality.pHalfOverFund;
    if (!r.quality.qualityWarning.empty()) {
        os << ",\"qualityWarning\":\"";
        // naive string escape for quotes/backslashes
        for (char c : r.quality.qualityWarning) { if (c=='"' || c=='\\') os << '\\'; os << c; }
        os << "\"";
    }
    os << "}";
    // binary segments
    os << ",\"binarySegments\":[";
    for (size_t i=0;i<r.binarySegments.size();++i){
        if(i) os << ",";
        const auto &bs = r.binarySegments[i];
        os << "{"
           << "\"index\":" << bs.index << ","
           << "\"startBeat\":" << bs.startBeat << ","
           << "\"endBeat\":" << bs.endBeat << ","
           << "\"totalBeats\":" << bs.totalBeats << ","
           << "\"rejectedBeats\":" << bs.rejectedBeats << ","
           << "\"accepted\":" << (bs.accepted?"true":"false")
           << "}";
    }
    os << "]";
    if (includeSegments) {
        os << ",\"segments\":[";
        for (size_t i=0;i<r.segments.size();++i){ if(i) os << ","; os << to_json(r.segments[i], false); }
        os << "]";
    }
    os << "}";
    return os.str();
}

static heartpy::Options buildOptions(
        double lowHz,
        double highHz,
        int order,
        int nfft,
        double overlap,
        double welchWsizeSec,
        double refractoryMs,
        double thresholdScale,
        double bpmMin,
        double bpmMax,
        jboolean interpClipping,
        double clippingThreshold,
        jboolean hampelCorrect,
        int hampelWindow,
        double hampelThreshold,
        jboolean removeBaselineWander,
        jboolean enhancePeaks,
        jboolean highPrecision,
        double highPrecisionFs,
        jboolean rejectSegmentwise,
        double segmentRejectThreshold,
        int segmentRejectMaxRejects,
        int segmentRejectWindowBeats,
        double segmentRejectOverlap,
        jboolean cleanRR,
        int cleanMethod,
        double segmentWidth,
        double segmentOverlap,
        double segmentMinSize,
        jboolean replaceOutliers,
        double rrSplineS,
        double rrSplineTargetSse,
        double rrSplineSmooth,
        jboolean breathingAsBpm,
        int sdsdMode,
        int poincareMode,
        jboolean pnnAsPercent,
        double snrTauSec,
        double snrActiveTauSec,
        jboolean adaptivePsd,
        jboolean thresholdRR,
        jboolean calcFreq,
        int filterMode) {
    heartpy::Options opt;
    opt.lowHz = lowHz;
    opt.highHz = highHz;
    opt.iirOrder = order;
    opt.nfft = nfft;
    opt.overlap = overlap;
    opt.welchWsizeSec = welchWsizeSec;
    opt.refractoryMs = refractoryMs;
    opt.thresholdScale = thresholdScale;
    opt.bpmMin = bpmMin;
    opt.bpmMax = bpmMax;
    opt.interpClipping = (interpClipping == JNI_TRUE);
    opt.clippingThreshold = clippingThreshold;
    opt.hampelCorrect = (hampelCorrect == JNI_TRUE);
    opt.hampelWindow = hampelWindow;
    opt.hampelThreshold = hampelThreshold;
    opt.removeBaselineWander = (removeBaselineWander == JNI_TRUE);
    opt.enhancePeaks = (enhancePeaks == JNI_TRUE);
    opt.highPrecision = (highPrecision == JNI_TRUE);
    opt.highPrecisionFs = highPrecisionFs;
    opt.rejectSegmentwise = (rejectSegmentwise == JNI_TRUE);
    opt.segmentRejectThreshold = segmentRejectThreshold;
    opt.segmentRejectMaxRejects = segmentRejectMaxRejects;
    opt.segmentRejectWindowBeats = segmentRejectWindowBeats;
    opt.segmentRejectOverlap = segmentRejectOverlap;
    opt.cleanRR = (cleanRR == JNI_TRUE);
    opt.cleanMethod = (cleanMethod == 1
            ? heartpy::Options::CleanMethod::IQR
            : (cleanMethod == 2
                    ? heartpy::Options::CleanMethod::Z_SCORE
                    : heartpy::Options::CleanMethod::QUOTIENT_FILTER));
    opt.segmentWidth = segmentWidth;
    opt.segmentOverlap = segmentOverlap;
    opt.segmentMinSize = segmentMinSize;
    opt.replaceOutliers = (replaceOutliers == JNI_TRUE);
    opt.rrSplineS = rrSplineS;
    opt.rrSplineSTargetSse = rrSplineTargetSse;
    opt.rrSplineSmooth = rrSplineSmooth;
    opt.breathingAsBpm = (breathingAsBpm == JNI_TRUE);
    opt.sdsdMode = (sdsdMode == 0
            ? heartpy::Options::SdsdMode::SIGNED
            : heartpy::Options::SdsdMode::ABS);
    opt.poincareMode = (poincareMode == 1
            ? heartpy::Options::PoincareMode::MASKED
            : heartpy::Options::PoincareMode::FORMULA);
    opt.pnnAsPercent = (pnnAsPercent == JNI_TRUE);
    opt.snrTauSec = snrTauSec;
    opt.snrActiveTauSec = snrActiveTauSec;
    opt.adaptivePsd = (adaptivePsd == JNI_TRUE);
    opt.thresholdRR = (thresholdRR == JNI_TRUE);
    opt.calcFreq = (calcFreq == JNI_TRUE);
    opt.filterMode = (filterMode == 1
            ? heartpy::Options::FilterMode::RBJ
            : (filterMode == 2
                    ? heartpy::Options::FilterMode::BUTTER_FILTFILT
                    : heartpy::Options::FilterMode::AUTO));
    return opt;
}

static jdoubleArray toJDoubleArray(JNIEnv* env, const std::vector<double>& values) {
    jsize len = static_cast<jsize>(values.size());
    jdoubleArray array = env->NewDoubleArray(len);
    if (array && len > 0) {
        env->SetDoubleArrayRegion(array, 0, len, values.data());
    }
    return array;
}

static jintArray toJIntArray(JNIEnv* env, const std::vector<int>& values) {
    jsize len = static_cast<jsize>(values.size());
    jintArray array = env->NewIntArray(len);
    if (array && len > 0) {
        env->SetIntArrayRegion(array, 0, len, reinterpret_cast<const jint*>(values.data()));
    }
    return array;
}

struct TypedClassCache {
    jclass metricsCls = nullptr;
    jmethodID metricsCtor = nullptr;
    jfieldID bpmField = nullptr;
    jfieldID sdnnField = nullptr;
    jfieldID rmssdField = nullptr;
    jfieldID sdsdField = nullptr;
    jfieldID pnn20Field = nullptr;
    jfieldID pnn50Field = nullptr;
    jfieldID nn20Field = nullptr;
    jfieldID nn50Field = nullptr;
    jfieldID madField = nullptr;
    jfieldID sd1Field = nullptr;
    jfieldID sd2Field = nullptr;
    jfieldID sd1sd2RatioField = nullptr;
    jfieldID ellipseAreaField = nullptr;
    jfieldID vlfField = nullptr;
    jfieldID lfField = nullptr;
    jfieldID hfField = nullptr;
    jfieldID lfhfField = nullptr;
    jfieldID totalPowerField = nullptr;
    jfieldID lfNormField = nullptr;
    jfieldID hfNormField = nullptr;
    jfieldID breathingRateField = nullptr;
    jfieldID ibiMsField = nullptr;
    jfieldID rrListField = nullptr;
    jfieldID peakListField = nullptr;
    jfieldID peakListRawField = nullptr;
    jfieldID binaryPeakMaskField = nullptr;
    jfieldID peakTimestampsField = nullptr;
    jfieldID waveformValuesField = nullptr;
    jfieldID waveformTimestampsField = nullptr;
    jfieldID binarySegmentsField = nullptr;
    jfieldID qualityField = nullptr;

    jclass qualityCls = nullptr;
    jmethodID qualityCtor = nullptr;
    jfieldID qualityTotalBeatsField = nullptr;
    jfieldID qualityRejectedBeatsField = nullptr;
    jfieldID qualityRejectionRateField = nullptr;
    jfieldID qualityGoodField = nullptr;
    jfieldID qualitySnrDbField = nullptr;
    jfieldID qualityConfidenceField = nullptr;
    jfieldID qualityF0HzField = nullptr;
    jfieldID qualityMaPercField = nullptr;
    jfieldID qualityDoublingFlagField = nullptr;
    jfieldID qualitySoftDoublingFlagField = nullptr;
    jfieldID qualityDoublingHintFlagField = nullptr;
    jfieldID qualityHardFallbackField = nullptr;
    jfieldID qualityRrFallbackField = nullptr;
    jfieldID qualitySnrWarmupField = nullptr;
    jfieldID qualitySnrSampleCountField = nullptr;
    jfieldID qualityRefractoryField = nullptr;
    jfieldID qualityMinRRBoundField = nullptr;
    jfieldID qualityPairFracField = nullptr;
    jfieldID qualityRrShortFracField = nullptr;
    jfieldID qualityRrLongMsField = nullptr;
    jfieldID qualityPHalfOverFundField = nullptr;
    jfieldID qualityWarningField = nullptr;

    jclass binarySegmentCls = nullptr;
    jmethodID binarySegmentCtor = nullptr;
    jfieldID binarySegmentIndexField = nullptr;
    jfieldID binarySegmentStartField = nullptr;
    jfieldID binarySegmentEndField = nullptr;
    jfieldID binarySegmentTotalBeatsField = nullptr;
    jfieldID binarySegmentRejectedBeatsField = nullptr;
    jfieldID binarySegmentAcceptedField = nullptr;

    void ensure(JNIEnv* env) {
        if (metricsCls != nullptr) {
            return;
        }
        jclass localMetrics = env->FindClass("com/heartpy/HeartPyModule$HeartMetricsTyped");
        metricsCls = static_cast<jclass>(env->NewGlobalRef(localMetrics));
        env->DeleteLocalRef(localMetrics);
        metricsCtor = env->GetMethodID(metricsCls, "<init>", "()V");
        bpmField = env->GetFieldID(metricsCls, "bpm", "D");
        sdnnField = env->GetFieldID(metricsCls, "sdnn", "D");
        rmssdField = env->GetFieldID(metricsCls, "rmssd", "D");
        sdsdField = env->GetFieldID(metricsCls, "sdsd", "D");
        pnn20Field = env->GetFieldID(metricsCls, "pnn20", "D");
        pnn50Field = env->GetFieldID(metricsCls, "pnn50", "D");
        nn20Field = env->GetFieldID(metricsCls, "nn20", "D");
        nn50Field = env->GetFieldID(metricsCls, "nn50", "D");
        madField = env->GetFieldID(metricsCls, "mad", "D");
        sd1Field = env->GetFieldID(metricsCls, "sd1", "D");
        sd2Field = env->GetFieldID(metricsCls, "sd2", "D");
        sd1sd2RatioField = env->GetFieldID(metricsCls, "sd1sd2Ratio", "D");
        ellipseAreaField = env->GetFieldID(metricsCls, "ellipseArea", "D");
        vlfField = env->GetFieldID(metricsCls, "vlf", "D");
        lfField = env->GetFieldID(metricsCls, "lf", "D");
        hfField = env->GetFieldID(metricsCls, "hf", "D");
        lfhfField = env->GetFieldID(metricsCls, "lfhf", "D");
        totalPowerField = env->GetFieldID(metricsCls, "totalPower", "D");
        lfNormField = env->GetFieldID(metricsCls, "lfNorm", "D");
        hfNormField = env->GetFieldID(metricsCls, "hfNorm", "D");
        breathingRateField = env->GetFieldID(metricsCls, "breathingRate", "D");
        ibiMsField = env->GetFieldID(metricsCls, "ibiMs", "[D");
        rrListField = env->GetFieldID(metricsCls, "rrList", "[D");
        peakListField = env->GetFieldID(metricsCls, "peakList", "[I");
        peakListRawField = env->GetFieldID(metricsCls, "peakListRaw", "[I");
        binaryPeakMaskField = env->GetFieldID(metricsCls, "binaryPeakMask", "[I");
        peakTimestampsField = env->GetFieldID(metricsCls, "peakTimestamps", "[D");
        waveformValuesField = env->GetFieldID(metricsCls, "waveform_values", "[D");
        waveformTimestampsField = env->GetFieldID(metricsCls, "waveform_timestamps", "[D");
        binarySegmentsField = env->GetFieldID(metricsCls, "binarySegments", "[Lcom/heartpy/HeartPyModule$BinarySegmentTyped;");
        qualityField = env->GetFieldID(metricsCls, "quality", "Lcom/heartpy/HeartPyModule$QualityTyped;");

        jclass localQuality = env->FindClass("com/heartpy/HeartPyModule$QualityTyped");
        qualityCls = static_cast<jclass>(env->NewGlobalRef(localQuality));
        env->DeleteLocalRef(localQuality);
        qualityCtor = env->GetMethodID(qualityCls, "<init>", "()V");
        qualityTotalBeatsField = env->GetFieldID(qualityCls, "totalBeats", "D");
        qualityRejectedBeatsField = env->GetFieldID(qualityCls, "rejectedBeats", "D");
        qualityRejectionRateField = env->GetFieldID(qualityCls, "rejectionRate", "D");
        qualityGoodField = env->GetFieldID(qualityCls, "goodQuality", "Z");
        qualitySnrDbField = env->GetFieldID(qualityCls, "snrDb", "D");
        qualityConfidenceField = env->GetFieldID(qualityCls, "confidence", "D");
        qualityF0HzField = env->GetFieldID(qualityCls, "f0Hz", "D");
        qualityMaPercField = env->GetFieldID(qualityCls, "maPercActive", "D");
        qualityDoublingFlagField = env->GetFieldID(qualityCls, "doublingFlag", "D");
        qualitySoftDoublingFlagField = env->GetFieldID(qualityCls, "softDoublingFlag", "D");
        qualityDoublingHintFlagField = env->GetFieldID(qualityCls, "doublingHintFlag", "D");
        qualityHardFallbackField = env->GetFieldID(qualityCls, "hardFallbackActive", "D");
        qualityRrFallbackField = env->GetFieldID(qualityCls, "rrFallbackModeActive", "D");
        qualitySnrWarmupField = env->GetFieldID(qualityCls, "snrWarmupActive", "D");
        qualitySnrSampleCountField = env->GetFieldID(qualityCls, "snrSampleCount", "D");
        qualityRefractoryField = env->GetFieldID(qualityCls, "refractoryMsActive", "D");
        qualityMinRRBoundField = env->GetFieldID(qualityCls, "minRRBoundMs", "D");
        qualityPairFracField = env->GetFieldID(qualityCls, "pairFrac", "D");
        qualityRrShortFracField = env->GetFieldID(qualityCls, "rrShortFrac", "D");
        qualityRrLongMsField = env->GetFieldID(qualityCls, "rrLongMs", "D");
        qualityPHalfOverFundField = env->GetFieldID(qualityCls, "pHalfOverFund", "D");
        qualityWarningField = env->GetFieldID(qualityCls, "qualityWarning", "Ljava/lang/String;");

        jclass localSegment = env->FindClass("com/heartpy/HeartPyModule$BinarySegmentTyped");
        binarySegmentCls = static_cast<jclass>(env->NewGlobalRef(localSegment));
        env->DeleteLocalRef(localSegment);
        binarySegmentCtor = env->GetMethodID(binarySegmentCls, "<init>", "()V");
        binarySegmentIndexField = env->GetFieldID(binarySegmentCls, "index", "I");
        binarySegmentStartField = env->GetFieldID(binarySegmentCls, "startBeat", "I");
        binarySegmentEndField = env->GetFieldID(binarySegmentCls, "endBeat", "I");
        binarySegmentTotalBeatsField = env->GetFieldID(binarySegmentCls, "totalBeats", "I");
        binarySegmentRejectedBeatsField = env->GetFieldID(binarySegmentCls, "rejectedBeats", "I");
        binarySegmentAcceptedField = env->GetFieldID(binarySegmentCls, "accepted", "Z");
    }
};

static TypedClassCache& getTypedClassCache(JNIEnv* env) {
    static TypedClassCache cache;
    cache.ensure(env);
    return cache;
}

static jobjectArray createBinarySegmentsArray(JNIEnv* env, TypedClassCache& cache, const std::vector<heartpy::HeartMetrics::BinarySegment>& segments) {
    jsize len = static_cast<jsize>(segments.size());
    jobjectArray array = env->NewObjectArray(len, cache.binarySegmentCls, nullptr);
    for (jsize i = 0; i < len; ++i) {
        jobject segObj = env->NewObject(cache.binarySegmentCls, cache.binarySegmentCtor);
        const auto& seg = segments[static_cast<size_t>(i)];
        env->SetIntField(segObj, cache.binarySegmentIndexField, seg.index);
        env->SetIntField(segObj, cache.binarySegmentStartField, seg.startBeat);
        env->SetIntField(segObj, cache.binarySegmentEndField, seg.endBeat);
        env->SetIntField(segObj, cache.binarySegmentTotalBeatsField, seg.totalBeats);
        env->SetIntField(segObj, cache.binarySegmentRejectedBeatsField, seg.rejectedBeats);
        env->SetBooleanField(segObj, cache.binarySegmentAcceptedField, seg.accepted ? JNI_TRUE : JNI_FALSE);
        env->SetObjectArrayElement(array, i, segObj);
        env->DeleteLocalRef(segObj);
    }
    return array;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_heartpy_HeartPyModule_analyzeNativeJson(
        JNIEnv* env,
        jclass,
        jdoubleArray jSignal,
        jdouble fs,
        jdouble lowHz,
        jdouble highHz,
        jint order,
        jint nfft,
        jdouble overlap,
        jdouble welchWsizeSec,
        jdouble refractoryMs,
        jdouble thresholdScale,
        jdouble bpmMin,
        jdouble bpmMax,
        jboolean interpClipping,
        jdouble clippingThreshold,
        jboolean hampelCorrect,
        jint hampelWindow,
        jdouble hampelThreshold,
        jboolean removeBaselineWander,
        jboolean enhancePeaks,
        jboolean highPrecision,
        jdouble highPrecisionFs,
        jboolean rejectSegmentwise,
        jdouble segmentRejectThreshold,
        jint segmentRejectMaxRejects,
        jint segmentRejectWindowBeats,
        jdouble segmentRejectOverlap,
        jboolean cleanRR,
        jint cleanMethod,
        jdouble segmentWidth,
        jdouble segmentOverlap,
        jdouble segmentMinSize,
        jboolean replaceOutliers,
        jdouble rrSplineS,
        jdouble rrSplineTargetSse,
        jdouble rrSplineSmooth,
        jboolean breathingAsBpm,
        jint sdsdMode,
        jint poincareMode,
        jboolean pnnAsPercent,
        jdouble snrTauSec,
        jdouble snrActiveTauSec,
        jboolean adaptivePsd,
        jboolean thresholdRR,
        jboolean calcFreq,
        jint filterMode) {
    jsize len = env->GetArrayLength(jSignal);
    std::vector<double> signal(len);
    env->GetDoubleArrayRegion(jSignal, 0, len, signal.data());

    heartpy::Options opt = buildOptions(
            lowHz,
            highHz,
            order,
            nfft,
            overlap,
            welchWsizeSec,
            refractoryMs,
            thresholdScale,
            bpmMin,
            bpmMax,
            interpClipping,
            clippingThreshold,
            hampelCorrect,
            hampelWindow,
            hampelThreshold,
            removeBaselineWander,
            enhancePeaks,
            highPrecision,
            highPrecisionFs,
            rejectSegmentwise,
            segmentRejectThreshold,
            segmentRejectMaxRejects,
            segmentRejectWindowBeats,
            segmentRejectOverlap,
            cleanRR,
            cleanMethod,
            segmentWidth,
            segmentOverlap,
            segmentMinSize,
            replaceOutliers,
            rrSplineS,
            rrSplineTargetSse,
            rrSplineSmooth,
            breathingAsBpm,
            sdsdMode,
            poincareMode,
            pnnAsPercent,
            snrTauSec,
            snrActiveTauSec,
            adaptivePsd,
            thresholdRR,
            calcFreq,
            filterMode);

    auto res = heartpy::analyzeSignal(signal, fs, opt);
    std::string json = to_json(res, false);
    return env->NewStringUTF(json.c_str());
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_heartpy_HeartPyModule_analyzeNativeTyped(
        JNIEnv* env,
        jclass,
        jdoubleArray jSignal,
        jdouble fs,
        jdouble lowHz,
        jdouble highHz,
        jint order,
        jint nfft,
        jdouble overlap,
        jdouble welchWsizeSec,
        jdouble refractoryMs,
        jdouble thresholdScale,
        jdouble bpmMin,
        jdouble bpmMax,
        jboolean interpClipping,
        jdouble clippingThreshold,
        jboolean hampelCorrect,
        jint hampelWindow,
        jdouble hampelThreshold,
        jboolean removeBaselineWander,
        jboolean enhancePeaks,
        jboolean highPrecision,
        jdouble highPrecisionFs,
        jboolean rejectSegmentwise,
        jdouble segmentRejectThreshold,
        jint segmentRejectMaxRejects,
        jint segmentRejectWindowBeats,
        jdouble segmentRejectOverlap,
        jboolean cleanRR,
        jint cleanMethod,
        jdouble segmentWidth,
        jdouble segmentOverlap,
        jdouble segmentMinSize,
        jboolean replaceOutliers,
        jdouble rrSplineS,
        jdouble rrSplineTargetSse,
        jdouble rrSplineSmooth,
        jboolean breathingAsBpm,
        jint sdsdMode,
        jint poincareMode,
        jboolean pnnAsPercent,
        jdouble snrTauSec,
        jdouble snrActiveTauSec,
        jboolean adaptivePsd,
        jboolean thresholdRR,
        jboolean calcFreq,
        jint filterMode) {
    jsize len = env->GetArrayLength(jSignal);
    std::vector<double> signal(len);
    env->GetDoubleArrayRegion(jSignal, 0, len, signal.data());

    heartpy::Options opt = buildOptions(
            lowHz,
            highHz,
            order,
            nfft,
            overlap,
            welchWsizeSec,
            refractoryMs,
            thresholdScale,
            bpmMin,
            bpmMax,
            interpClipping,
            clippingThreshold,
            hampelCorrect,
            hampelWindow,
            hampelThreshold,
            removeBaselineWander,
            enhancePeaks,
            highPrecision,
            highPrecisionFs,
            rejectSegmentwise,
            segmentRejectThreshold,
            segmentRejectMaxRejects,
            segmentRejectWindowBeats,
            segmentRejectOverlap,
            cleanRR,
            cleanMethod,
            segmentWidth,
            segmentOverlap,
            segmentMinSize,
            replaceOutliers,
            rrSplineS,
            rrSplineTargetSse,
            rrSplineSmooth,
            breathingAsBpm,
            sdsdMode,
            poincareMode,
            pnnAsPercent,
            snrTauSec,
            snrActiveTauSec,
            adaptivePsd,
            thresholdRR,
            calcFreq,
            filterMode);

    heartpy::HeartMetrics res = heartpy::analyzeSignal(signal, fs, opt);

    auto& cache = getTypedClassCache(env);
    jobject metricsObj = env->NewObject(cache.metricsCls, cache.metricsCtor);
    if (!metricsObj) {
        return nullptr;
    }

    env->SetDoubleField(metricsObj, cache.bpmField, res.bpm);
    env->SetDoubleField(metricsObj, cache.sdnnField, res.sdnn);
    env->SetDoubleField(metricsObj, cache.rmssdField, res.rmssd);
    env->SetDoubleField(metricsObj, cache.sdsdField, res.sdsd);
    env->SetDoubleField(metricsObj, cache.pnn20Field, res.pnn20);
    env->SetDoubleField(metricsObj, cache.pnn50Field, res.pnn50);
    env->SetDoubleField(metricsObj, cache.nn20Field, res.nn20);
    env->SetDoubleField(metricsObj, cache.nn50Field, res.nn50);
    env->SetDoubleField(metricsObj, cache.madField, res.mad);
    env->SetDoubleField(metricsObj, cache.sd1Field, res.sd1);
    env->SetDoubleField(metricsObj, cache.sd2Field, res.sd2);
    env->SetDoubleField(metricsObj, cache.sd1sd2RatioField, res.sd1sd2Ratio);
    env->SetDoubleField(metricsObj, cache.ellipseAreaField, res.ellipseArea);
    env->SetDoubleField(metricsObj, cache.vlfField, res.vlf);
    env->SetDoubleField(metricsObj, cache.lfField, res.lf);
    env->SetDoubleField(metricsObj, cache.hfField, res.hf);
    env->SetDoubleField(metricsObj, cache.lfhfField, res.lfhf);
    env->SetDoubleField(metricsObj, cache.totalPowerField, res.totalPower);
    env->SetDoubleField(metricsObj, cache.lfNormField, res.lfNorm);
    env->SetDoubleField(metricsObj, cache.hfNormField, res.hfNorm);
    env->SetDoubleField(metricsObj, cache.breathingRateField, res.breathingRate);

    jdoubleArray ibiArr = toJDoubleArray(env, res.ibiMs);
    env->SetObjectField(metricsObj, cache.ibiMsField, ibiArr);
    if (ibiArr) env->DeleteLocalRef(ibiArr);

    jdoubleArray rrArr = toJDoubleArray(env, res.rrList);
    env->SetObjectField(metricsObj, cache.rrListField, rrArr);
    if (rrArr) env->DeleteLocalRef(rrArr);

    jintArray peakList = toJIntArray(env, res.peakList);
    env->SetObjectField(metricsObj, cache.peakListField, peakList);
    if (peakList) env->DeleteLocalRef(peakList);

    jintArray peakListRaw = toJIntArray(env, res.peakListRaw);
    env->SetObjectField(metricsObj, cache.peakListRawField, peakListRaw);
    if (peakListRaw) env->DeleteLocalRef(peakListRaw);

    jintArray binaryMask = toJIntArray(env, res.binaryPeakMask);
    env->SetObjectField(metricsObj, cache.binaryPeakMaskField, binaryMask);
    if (binaryMask) env->DeleteLocalRef(binaryMask);

    jdoubleArray peakTsArr = toJDoubleArray(env, res.peakTimestamps);
    env->SetObjectField(metricsObj, cache.peakTimestampsField, peakTsArr);
    if (peakTsArr) env->DeleteLocalRef(peakTsArr);

    jdoubleArray waveformValues = toJDoubleArray(env, res.waveform_values);
    env->SetObjectField(metricsObj, cache.waveformValuesField, waveformValues);
    if (waveformValues) env->DeleteLocalRef(waveformValues);

    jdoubleArray waveformTs = toJDoubleArray(env, res.waveform_timestamps);
    env->SetObjectField(metricsObj, cache.waveformTimestampsField, waveformTs);
    if (waveformTs) env->DeleteLocalRef(waveformTs);

    jobject qualityObj = env->NewObject(cache.qualityCls, cache.qualityCtor);
    if (qualityObj) {
        const auto& q = res.quality;
        env->SetDoubleField(qualityObj, cache.qualityTotalBeatsField, static_cast<double>(q.totalBeats));
        env->SetDoubleField(qualityObj, cache.qualityRejectedBeatsField, static_cast<double>(q.rejectedBeats));
        env->SetDoubleField(qualityObj, cache.qualityRejectionRateField, q.rejectionRate);
        env->SetBooleanField(qualityObj, cache.qualityGoodField, q.goodQuality ? JNI_TRUE : JNI_FALSE);
        env->SetDoubleField(qualityObj, cache.qualitySnrDbField, q.snrDb);
        env->SetDoubleField(qualityObj, cache.qualityConfidenceField, q.confidence);
        env->SetDoubleField(qualityObj, cache.qualityF0HzField, q.f0Hz);
        env->SetDoubleField(qualityObj, cache.qualityMaPercField, q.maPercActive);
        env->SetDoubleField(qualityObj, cache.qualityDoublingFlagField, static_cast<double>(q.doublingFlag));
        env->SetDoubleField(qualityObj, cache.qualitySoftDoublingFlagField, static_cast<double>(q.softDoublingFlag));
        env->SetDoubleField(qualityObj, cache.qualityDoublingHintFlagField, static_cast<double>(q.doublingHintFlag));
        env->SetDoubleField(qualityObj, cache.qualityHardFallbackField, static_cast<double>(q.hardFallbackActive));
        env->SetDoubleField(qualityObj, cache.qualityRrFallbackField, static_cast<double>(q.rrFallbackModeActive));
        env->SetDoubleField(qualityObj, cache.qualitySnrWarmupField, static_cast<double>(q.snrWarmupActive));
        env->SetDoubleField(qualityObj, cache.qualitySnrSampleCountField, q.snrSampleCount);
        env->SetDoubleField(qualityObj, cache.qualityRefractoryField, q.refractoryMsActive);
        env->SetDoubleField(qualityObj, cache.qualityMinRRBoundField, q.minRRBoundMs);
        env->SetDoubleField(qualityObj, cache.qualityPairFracField, q.pairFrac);
        env->SetDoubleField(qualityObj, cache.qualityRrShortFracField, q.rrShortFrac);
        env->SetDoubleField(qualityObj, cache.qualityRrLongMsField, q.rrLongMs);
        env->SetDoubleField(qualityObj, cache.qualityPHalfOverFundField, q.pHalfOverFund);
        if (!q.qualityWarning.empty()) {
            jstring warning = env->NewStringUTF(q.qualityWarning.c_str());
            env->SetObjectField(qualityObj, cache.qualityWarningField, warning);
            env->DeleteLocalRef(warning);
        }
        env->SetObjectField(metricsObj, cache.qualityField, qualityObj);
        env->DeleteLocalRef(qualityObj);
    }

    jobjectArray segmentsArray = createBinarySegmentsArray(env, cache, res.binarySegments);
    env->SetObjectField(metricsObj, cache.binarySegmentsField, segmentsArray);
    if (segmentsArray) env->DeleteLocalRef(segmentsArray);

    return metricsObj;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_heartpy_HeartPyModule_analyzeRRNativeJson(
        JNIEnv* env,
        jclass,
        jdoubleArray jRR,
        jboolean cleanRR,
        jint cleanMethod,
        jboolean breathingAsBpm,
        jboolean thresholdRR,
        jint sdsdMode,
        jint poincareMode,
        jboolean pnnAsPercent) {
    jsize len = env->GetArrayLength(jRR);
    std::vector<double> rr(len);
    env->GetDoubleArrayRegion(jRR, 0, len, rr.data());
    heartpy::Options opt;
    opt.cleanRR = cleanRR; opt.cleanMethod = (cleanMethod==1? heartpy::Options::CleanMethod::IQR : (cleanMethod==2? heartpy::Options::CleanMethod::Z_SCORE : heartpy::Options::CleanMethod::QUOTIENT_FILTER));
    opt.breathingAsBpm = breathingAsBpm;
    opt.thresholdRR = (thresholdRR==JNI_TRUE);
    opt.sdsdMode = (sdsdMode==0 ? heartpy::Options::SdsdMode::SIGNED : heartpy::Options::SdsdMode::ABS);
    opt.poincareMode = (poincareMode==1 ? heartpy::Options::PoincareMode::MASKED : heartpy::Options::PoincareMode::FORMULA);
    opt.pnnAsPercent = (pnnAsPercent==JNI_TRUE);
    auto res = heartpy::analyzeRRIntervals(rr, opt);
    std::string json = to_json(res, false);
    return env->NewStringUTF(json.c_str());
}

// ----- Typed Helpers and Typed JNI for Segmentwise / RR -----
static jobject makeTypedMetrics(JNIEnv* env, const heartpy::HeartMetrics& res) {
    auto& cache = getTypedClassCache(env);
    jobject metricsObj = env->NewObject(cache.metricsCls, cache.metricsCtor);
    if (!metricsObj) return nullptr;

    // Scalars
    env->SetDoubleField(metricsObj, cache.bpmField, res.bpm);
    env->SetDoubleField(metricsObj, cache.sdnnField, res.sdnn);
    env->SetDoubleField(metricsObj, cache.rmssdField, res.rmssd);
    env->SetDoubleField(metricsObj, cache.sdsdField, res.sdsd);
    env->SetDoubleField(metricsObj, cache.pnn20Field, res.pnn20);
    env->SetDoubleField(metricsObj, cache.pnn50Field, res.pnn50);
    env->SetDoubleField(metricsObj, cache.nn20Field, res.nn20);
    env->SetDoubleField(metricsObj, cache.nn50Field, res.nn50);
    env->SetDoubleField(metricsObj, cache.madField, res.mad);
    env->SetDoubleField(metricsObj, cache.sd1Field, res.sd1);
    env->SetDoubleField(metricsObj, cache.sd2Field, res.sd2);
    env->SetDoubleField(metricsObj, cache.sd1sd2RatioField, res.sd1sd2Ratio);
    env->SetDoubleField(metricsObj, cache.ellipseAreaField, res.ellipseArea);
    env->SetDoubleField(metricsObj, cache.vlfField, res.vlf);
    env->SetDoubleField(metricsObj, cache.lfField, res.lf);
    env->SetDoubleField(metricsObj, cache.hfField, res.hf);
    env->SetDoubleField(metricsObj, cache.lfhfField, res.lfhf);
    env->SetDoubleField(metricsObj, cache.totalPowerField, res.totalPower);
    env->SetDoubleField(metricsObj, cache.lfNormField, res.lfNorm);
    env->SetDoubleField(metricsObj, cache.hfNormField, res.hfNorm);
    env->SetDoubleField(metricsObj, cache.breathingRateField, res.breathingRate);

    // Arrays
    jdoubleArray ibiArr = toJDoubleArray(env, res.ibiMs); env->SetObjectField(metricsObj, cache.ibiMsField, ibiArr); if (ibiArr) env->DeleteLocalRef(ibiArr);
    jdoubleArray rrArr = toJDoubleArray(env, res.rrList); env->SetObjectField(metricsObj, cache.rrListField, rrArr); if (rrArr) env->DeleteLocalRef(rrArr);
    jintArray peakList = toJIntArray(env, res.peakList); env->SetObjectField(metricsObj, cache.peakListField, peakList); if (peakList) env->DeleteLocalRef(peakList);
    jintArray peakListRaw = toJIntArray(env, res.peakListRaw); env->SetObjectField(metricsObj, cache.peakListRawField, peakListRaw); if (peakListRaw) env->DeleteLocalRef(peakListRaw);
    jintArray binaryMask = toJIntArray(env, res.binaryPeakMask); env->SetObjectField(metricsObj, cache.binaryPeakMaskField, binaryMask); if (binaryMask) env->DeleteLocalRef(binaryMask);
    jdoubleArray peakTsArr = toJDoubleArray(env, res.peakTimestamps); env->SetObjectField(metricsObj, cache.peakTimestampsField, peakTsArr); if (peakTsArr) env->DeleteLocalRef(peakTsArr);
    jdoubleArray waveformValues = toJDoubleArray(env, res.waveform_values); env->SetObjectField(metricsObj, cache.waveformValuesField, waveformValues); if (waveformValues) env->DeleteLocalRef(waveformValues);
    jdoubleArray waveformTs = toJDoubleArray(env, res.waveform_timestamps); env->SetObjectField(metricsObj, cache.waveformTimestampsField, waveformTs); if (waveformTs) env->DeleteLocalRef(waveformTs);

    // Quality
    jobject q = env->NewObject(cache.qualityCls, cache.qualityCtor);
    if (q) {
        const auto& qu = res.quality;
        env->SetDoubleField(q, cache.qualityTotalBeatsField, static_cast<double>(qu.totalBeats));
        env->SetDoubleField(q, cache.qualityRejectedBeatsField, static_cast<double>(qu.rejectedBeats));
        env->SetDoubleField(q, cache.qualityRejectionRateField, qu.rejectionRate);
        env->SetBooleanField(q, cache.qualityGoodField, qu.goodQuality ? JNI_TRUE : JNI_FALSE);
        env->SetDoubleField(q, cache.qualitySnrDbField, qu.snrDb);
        env->SetDoubleField(q, cache.qualityConfidenceField, qu.confidence);
        env->SetDoubleField(q, cache.qualityF0HzField, qu.f0Hz);
        env->SetDoubleField(q, cache.qualityMaPercField, qu.maPercActive);
        env->SetDoubleField(q, cache.qualityDoublingFlagField, static_cast<double>(qu.doublingFlag));
        env->SetDoubleField(q, cache.qualitySoftDoublingFlagField, static_cast<double>(qu.softDoublingFlag));
        env->SetDoubleField(q, cache.qualityDoublingHintFlagField, static_cast<double>(qu.doublingHintFlag));
        env->SetDoubleField(q, cache.qualityHardFallbackField, static_cast<double>(qu.hardFallbackActive));
        env->SetDoubleField(q, cache.qualityRrFallbackField, static_cast<double>(qu.rrFallbackModeActive));
        env->SetDoubleField(q, cache.qualitySnrWarmupField, static_cast<double>(qu.snrWarmupActive));
        env->SetDoubleField(q, cache.qualitySnrSampleCountField, qu.snrSampleCount);
        env->SetDoubleField(q, cache.qualityRefractoryField, qu.refractoryMsActive);
        env->SetDoubleField(q, cache.qualityMinRRBoundField, qu.minRRBoundMs);
        env->SetDoubleField(q, cache.qualityPairFracField, qu.pairFrac);
        env->SetDoubleField(q, cache.qualityRrShortFracField, qu.rrShortFrac);
        env->SetDoubleField(q, cache.qualityRrLongMsField, qu.rrLongMs);
        env->SetDoubleField(q, cache.qualityPHalfOverFundField, qu.pHalfOverFund);
        if (!qu.qualityWarning.empty()) {
            jstring s = env->NewStringUTF(qu.qualityWarning.c_str());
            env->SetObjectField(q, cache.qualityWarningField, s);
            env->DeleteLocalRef(s);
        }
        env->SetObjectField(metricsObj, cache.qualityField, q);
        env->DeleteLocalRef(q);
    }

    jobjectArray segs = createBinarySegmentsArray(env, cache, res.binarySegments);
    env->SetObjectField(metricsObj, cache.binarySegmentsField, segs);
    if (segs) env->DeleteLocalRef(segs);
    return metricsObj;
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_heartpy_HeartPyModule_analyzeSegmentwiseNativeTyped(
        JNIEnv* env,
        jclass,
        jdoubleArray jSignal,
        jdouble fs,
        jdouble lowHz,
        jdouble highHz,
        jint order,
        jint nfft,
        jdouble overlap,
        jdouble welchWsizeSec,
        jdouble refractoryMs,
        jdouble thresholdScale,
        jdouble bpmMin,
        jdouble bpmMax,
        jboolean interpClipping,
        jdouble clippingThreshold,
        jboolean hampelCorrect,
        jint hampelWindow,
        jdouble hampelThreshold,
        jboolean removeBaselineWander,
        jboolean enhancePeaks,
        jboolean highPrecision,
        jdouble highPrecisionFs,
        jboolean rejectSegmentwise,
        jdouble segmentRejectThreshold,
        jint segmentRejectMaxRejects,
        jint segmentRejectWindowBeats,
        jdouble segmentRejectOverlap,
        jboolean cleanRR,
        jint cleanMethod,
        jdouble segmentWidth,
        jdouble segmentOverlap,
        jdouble segmentMinSize,
        jboolean replaceOutliers,
        jdouble rrSplineS,
        jdouble rrSplineTargetSse,
        jdouble rrSplineSmooth,
        jboolean breathingAsBpm,
        jint sdsdMode,
        jint poincareMode,
        jboolean pnnAsPercent,
        jdouble snrTauSec,
        jdouble snrActiveTauSec,
        jboolean adaptivePsd,
        jboolean thresholdRR,
        jboolean calcFreq,
        jint filterMode) {
    jsize len = env->GetArrayLength(jSignal);
    std::vector<double> signal(len);
    env->GetDoubleArrayRegion(jSignal, 0, len, signal.data());
    heartpy::Options opt = buildOptions(lowHz, highHz, order, nfft, overlap, welchWsizeSec, refractoryMs, thresholdScale, bpmMin, bpmMax,
        interpClipping, clippingThreshold, hampelCorrect, hampelWindow, hampelThreshold, removeBaselineWander, enhancePeaks,
        highPrecision, highPrecisionFs, rejectSegmentwise, segmentRejectThreshold, segmentRejectMaxRejects, segmentRejectWindowBeats, segmentRejectOverlap,
        cleanRR, cleanMethod, segmentWidth, segmentOverlap, segmentMinSize, replaceOutliers, rrSplineS, rrSplineTargetSse, rrSplineSmooth,
        breathingAsBpm, sdsdMode, poincareMode, pnnAsPercent, snrTauSec, snrActiveTauSec, adaptivePsd, thresholdRR, calcFreq, filterMode);
    auto res = heartpy::analyzeSignalSegmentwise(signal, fs, opt);
    return makeTypedMetrics(env, res);
}

extern "C" JNIEXPORT jobject JNICALL
Java_com_heartpy_HeartPyModule_analyzeRRNativeTyped(
        JNIEnv* env,
        jclass,
        jdoubleArray jRR,
        jboolean cleanRR,
        jint cleanMethod,
        jboolean breathingAsBpm,
        jboolean thresholdRR,
        jint sdsdMode,
        jint poincareMode,
        jboolean pnnAsPercent) {
    jsize len = env->GetArrayLength(jRR);
    std::vector<double> rr(len);
    env->GetDoubleArrayRegion(jRR, 0, len, rr.data());
    heartpy::Options opt;
    opt.cleanRR = cleanRR; opt.cleanMethod = (cleanMethod==1? heartpy::Options::CleanMethod::IQR : (cleanMethod==2? heartpy::Options::CleanMethod::Z_SCORE : heartpy::Options::CleanMethod::QUOTIENT_FILTER));
    opt.breathingAsBpm = breathingAsBpm;
    opt.thresholdRR = (thresholdRR==JNI_TRUE);
    opt.sdsdMode = (sdsdMode==0 ? heartpy::Options::SdsdMode::SIGNED : heartpy::Options::SdsdMode::ABS);
    opt.poincareMode = (poincareMode==1 ? heartpy::Options::PoincareMode::MASKED : heartpy::Options::PoincareMode::FORMULA);
    opt.pnnAsPercent = (pnnAsPercent==JNI_TRUE);
    auto res = heartpy::analyzeRRIntervals(rr, opt);
    return makeTypedMetrics(env, res);
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_heartpy_HeartPyModule_analyzeSegmentwiseNativeJson(
        JNIEnv* env,
        jclass,
        jdoubleArray jSignal,
        jdouble fs,
        jdouble lowHz,
        jdouble highHz,
        jint order,
        jint nfft,
        jdouble overlap,
        jdouble welchWsizeSec,
        jdouble refractoryMs,
        jdouble thresholdScale,
        jdouble bpmMin,
        jdouble bpmMax,
        jboolean interpClipping,
        jdouble clippingThreshold,
        jboolean hampelCorrect,
        jint hampelWindow,
        jdouble hampelThreshold,
        jboolean removeBaselineWander,
        jboolean enhancePeaks,
        jboolean highPrecision,
        jdouble highPrecisionFs,
        jboolean rejectSegmentwise,
        jdouble segmentRejectThreshold,
        jint segmentRejectMaxRejects,
        jint segmentRejectWindowBeats,
        jdouble segmentRejectOverlap,
        jboolean cleanRR,
        jint cleanMethod,
        jdouble segmentWidth,
        jdouble segmentOverlap,
        jdouble segmentMinSize,
        jboolean replaceOutliers,
        jdouble rrSplineS,
        jdouble rrSplineTargetSse,
        jdouble rrSplineSmooth,
        jboolean breathingAsBpm,
        jint sdsdMode,
        jint poincareMode,
        jboolean pnnAsPercent,
        jdouble snrTauSec,
        jdouble snrActiveTauSec,
        jboolean adaptivePsd,
        jboolean thresholdRR,
        jboolean calcFreq,
        jint filterMode) {
    jsize len = env->GetArrayLength(jSignal);
    std::vector<double> signal(len);
    env->GetDoubleArrayRegion(jSignal, 0, len, signal.data());
    heartpy::Options opt;
    opt.lowHz = lowHz; opt.highHz = highHz; opt.iirOrder = order;
    opt.nfft = nfft; opt.overlap = overlap; opt.welchWsizeSec = welchWsizeSec;
    opt.refractoryMs = refractoryMs; opt.thresholdScale = thresholdScale; opt.bpmMin = bpmMin; opt.bpmMax = bpmMax;
    opt.interpClipping = interpClipping; opt.clippingThreshold = clippingThreshold;
    opt.hampelCorrect = hampelCorrect; opt.hampelWindow = hampelWindow; opt.hampelThreshold = hampelThreshold;
    opt.removeBaselineWander = removeBaselineWander; opt.enhancePeaks = enhancePeaks;
    opt.highPrecision = highPrecision; opt.highPrecisionFs = highPrecisionFs;
    opt.rejectSegmentwise = rejectSegmentwise; opt.segmentRejectThreshold = segmentRejectThreshold; opt.segmentRejectMaxRejects = segmentRejectMaxRejects; opt.segmentRejectWindowBeats = segmentRejectWindowBeats;
    opt.cleanRR = cleanRR; opt.cleanMethod = (cleanMethod==1? heartpy::Options::CleanMethod::IQR : (cleanMethod==2? heartpy::Options::CleanMethod::Z_SCORE : heartpy::Options::CleanMethod::QUOTIENT_FILTER));
    opt.segmentWidth = segmentWidth; opt.segmentOverlap = segmentOverlap; opt.segmentMinSize = segmentMinSize; opt.replaceOutliers = replaceOutliers;
    opt.rrSplineS = rrSplineS; opt.rrSplineSTargetSse = rrSplineTargetSse; opt.rrSplineSmooth = rrSplineSmooth;
    opt.breathingAsBpm = breathingAsBpm;
    opt.sdsdMode = (sdsdMode==0 ? heartpy::Options::SdsdMode::SIGNED : heartpy::Options::SdsdMode::ABS);
    opt.poincareMode = (poincareMode==1 ? heartpy::Options::PoincareMode::MASKED : heartpy::Options::PoincareMode::FORMULA);
    opt.pnnAsPercent = (pnnAsPercent==JNI_TRUE);
    opt.snrTauSec = snrTauSec;
    opt.snrActiveTauSec = snrActiveTauSec;
    opt.adaptivePsd = (adaptivePsd == JNI_TRUE);
    opt.thresholdRR = (thresholdRR == JNI_TRUE);
    opt.calcFreq = (calcFreq == JNI_TRUE);
    opt.filterMode = (filterMode==1? heartpy::Options::FilterMode::RBJ : (filterMode==2? heartpy::Options::FilterMode::BUTTER_FILTFILT : heartpy::Options::FilterMode::AUTO));
    auto res = heartpy::analyzeSignalSegmentwise(signal, fs, opt);
    std::string json = to_json(res, true);
    return env->NewStringUTF(json.c_str());
}

extern "C" JNIEXPORT jdoubleArray JNICALL
Java_com_heartpy_HeartPyModule_interpolateClippingNative(JNIEnv* env, jclass, jdoubleArray jSignal, jdouble fs, jdouble threshold) {
    jsize len = env->GetArrayLength(jSignal);
    std::vector<double> signal(len);
    env->GetDoubleArrayRegion(jSignal, 0, len, signal.data());
    auto y = heartpy::interpolateClipping(signal, fs, threshold);
    jdoubleArray out = env->NewDoubleArray((jsize)y.size());
    if (!y.empty()) env->SetDoubleArrayRegion(out, 0, (jsize)y.size(), y.data());
    return out;
}

extern "C" JNIEXPORT jdoubleArray JNICALL
Java_com_heartpy_HeartPyModule_hampelFilterNative(JNIEnv* env, jclass, jdoubleArray jSignal, jint windowSize, jdouble threshold) {
    jsize len = env->GetArrayLength(jSignal);
    std::vector<double> signal(len);
    env->GetDoubleArrayRegion(jSignal, 0, len, signal.data());
    auto y = heartpy::hampelFilter(signal, windowSize, threshold);
    jdoubleArray out = env->NewDoubleArray((jsize)y.size());
    if (!y.empty()) env->SetDoubleArrayRegion(out, 0, (jsize)y.size(), y.data());
    return out;
}

extern "C" JNIEXPORT jdoubleArray JNICALL
Java_com_heartpy_HeartPyModule_scaleDataNative(JNIEnv* env, jclass, jdoubleArray jSignal, jdouble newMin, jdouble newMax) {
    jsize len = env->GetArrayLength(jSignal);
    std::vector<double> signal(len);
    env->GetDoubleArrayRegion(jSignal, 0, len, signal.data());
    auto y = heartpy::scaleData(signal, newMin, newMax);
    jdoubleArray out = env->NewDoubleArray((jsize)y.size());
    if (!y.empty()) env->SetDoubleArrayRegion(out, 0, (jsize)y.size(), y.data());
    return out;
}

// ------------------------------
// Realtime Streaming JNI (P0)
// ------------------------------

extern "C" JNIEXPORT jlong JNICALL
Java_com_heartpy_HeartPyModule_rtCreateNative(
        JNIEnv* env,
        jclass,
        jdouble fs,
        jdouble lowHz,
        jdouble highHz,
        jint order,
        jint nfft,
        jdouble overlap,
        jdouble welchWsizeSec,
        jdouble refractoryMs,
        jdouble thresholdScale,
        jdouble bpmMin,
        jdouble bpmMax,
        jboolean interpClipping,
        jdouble clippingThreshold,
        jboolean hampelCorrect,
        jint hampelWindow,
        jdouble hampelThreshold,
        jboolean removeBaselineWander,
        jboolean enhancePeaks,
        jboolean highPrecision,
        jdouble highPrecisionFs,
        jboolean rejectSegmentwise,
        jdouble segmentRejectThreshold,
        jint segmentRejectMaxRejects,
        jint segmentRejectWindowBeats,
        jdouble segmentRejectOverlap,
        jboolean cleanRR,
        jint cleanMethod,
        jdouble segmentWidth,
        jdouble segmentOverlap,
        jdouble segmentMinSize,
        jboolean replaceOutliers,
        jdouble rrSplineS,
        jdouble rrSplineTargetSse,
        jdouble rrSplineSmooth,
        jboolean breathingAsBpm,
        jint sdsdMode,
        jint poincareMode,
        jboolean pnnAsPercent,
        jdouble snrTauSec,
        jdouble snrActiveTauSec,
        jboolean thresholdRR,
        jboolean calcFreq,
        jint filterMode) {
    heartpy::Options opt;
    opt.lowHz = lowHz; opt.highHz = highHz; opt.iirOrder = order;
    opt.nfft = nfft; opt.overlap = overlap; opt.welchWsizeSec = welchWsizeSec;
    opt.refractoryMs = refractoryMs; opt.thresholdScale = thresholdScale; opt.bpmMin = bpmMin; opt.bpmMax = bpmMax;
    opt.interpClipping = interpClipping; opt.clippingThreshold = clippingThreshold;
    opt.hampelCorrect = hampelCorrect; opt.hampelWindow = hampelWindow; opt.hampelThreshold = hampelThreshold;
    opt.removeBaselineWander = removeBaselineWander; opt.enhancePeaks = enhancePeaks;
    opt.highPrecision = highPrecision; opt.highPrecisionFs = highPrecisionFs;
    opt.rejectSegmentwise = rejectSegmentwise; opt.segmentRejectThreshold = segmentRejectThreshold; opt.segmentRejectMaxRejects = segmentRejectMaxRejects; opt.segmentRejectWindowBeats = segmentRejectWindowBeats; opt.segmentRejectOverlap = segmentRejectOverlap;
    opt.cleanRR = cleanRR; opt.cleanMethod = (cleanMethod==1? heartpy::Options::CleanMethod::IQR : (cleanMethod==2? heartpy::Options::CleanMethod::Z_SCORE : heartpy::Options::CleanMethod::QUOTIENT_FILTER));
    opt.segmentWidth = segmentWidth; opt.segmentOverlap = segmentOverlap; opt.segmentMinSize = segmentMinSize; opt.replaceOutliers = replaceOutliers;
    opt.rrSplineS = rrSplineS; opt.rrSplineSTargetSse = rrSplineTargetSse; opt.rrSplineSmooth = rrSplineSmooth;
    opt.breathingAsBpm = breathingAsBpm;
    opt.sdsdMode = (sdsdMode==0 ? heartpy::Options::SdsdMode::SIGNED : heartpy::Options::SdsdMode::ABS);
    opt.poincareMode = (poincareMode==1 ? heartpy::Options::PoincareMode::MASKED : heartpy::Options::PoincareMode::FORMULA);
    opt.pnnAsPercent = (pnnAsPercent==JNI_TRUE);
    opt.snrTauSec = snrTauSec;
    opt.snrActiveTauSec = snrActiveTauSec;
    opt.thresholdRR = (thresholdRR == JNI_TRUE);
    opt.calcFreq = (calcFreq == JNI_TRUE);
    opt.filterMode = (filterMode==1? heartpy::Options::FilterMode::RBJ : (filterMode==2? heartpy::Options::FilterMode::BUTTER_FILTFILT : heartpy::Options::FilterMode::AUTO));
    void* h = hp_rt_create(fs, &opt);
    return (jlong)h;
}

extern "C" JNIEXPORT void JNICALL
Java_com_heartpy_HeartPyModule_rtPushNative(JNIEnv* env, jclass, jlong h, jdoubleArray jData, jdouble t0) {
    if (!h || !jData) return;
    jsize len = env->GetArrayLength(jData);
    if (len <= 0) return;
    std::vector<double> tmp(len);
    env->GetDoubleArrayRegion(jData, 0, len, tmp.data());
    std::vector<float> x(len);
    for (jsize i = 0; i < len; ++i) x[i] = static_cast<float>(tmp[i]);
    hp_rt_push((void*)h, x.data(), (size_t)x.size(), t0);
}

extern "C" JNIEXPORT void JNICALL
Java_com_heartpy_HeartPyModule_rtPushTsNative(JNIEnv* env, jclass, jlong h, jdoubleArray jData, jdoubleArray jTs) {
    if (!h || !jData || !jTs) return;
    jsize len = env->GetArrayLength(jData);
    jsize lt = env->GetArrayLength(jTs);
    if (len <= 0 || lt <= 0) return;
    jsize n = std::min(len, lt);
    std::vector<double> tmp(n), tsv(n);
    env->GetDoubleArrayRegion(jData, 0, n, tmp.data());
    env->GetDoubleArrayRegion(jTs, 0, n, tsv.data());
    std::vector<float> x(n);
    for (jsize i = 0; i < n; ++i) x[i] = static_cast<float>(tmp[i]);
    hp_rt_push_ts((void*)h, x.data(), tsv.data(), (size_t)n);
}

extern "C" JNIEXPORT void JNICALL
Java_com_heartpy_HeartPyModule_rtSetWindowNative(JNIEnv*, jclass, jlong h, jdouble windowSec) {
    if (!h) return;
    hp_rt_set_window((void*)h, windowSec);
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_heartpy_HeartPyModule_rtPollNative(JNIEnv* env, jclass, jlong h) {
    if (!h) return nullptr;
    heartpy::HeartMetrics out;
    if (!hp_rt_poll((void*)h, &out)) return nullptr;
    std::string json = to_json(out, false);
    return env->NewStringUTF(json.c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_com_heartpy_HeartPyModule_rtDestroyNative(JNIEnv* env, jclass, jlong h) {
    if (!h) return;
    hp_rt_destroy((void*)h);
}

// Validator JNI: returns error code string on failure, or null on success
extern "C" JNIEXPORT jstring JNICALL
Java_com_heartpy_HeartPyModule_rtValidateOptionsNative(
        JNIEnv* env,
        jclass,
        jdouble fs,
        jdouble lowHz,
        jdouble highHz,
        jint order,
        jint nfft,
        jdouble overlap,
        jdouble welchWsizeSec,
        jdouble refractoryMs,
        jdouble bpmMin,
        jdouble bpmMax,
        jdouble highPrecisionFs) {
    heartpy::Options opt;
    opt.lowHz = lowHz; opt.highHz = highHz; opt.iirOrder = order;
    opt.nfft = nfft; opt.overlap = overlap; opt.welchWsizeSec = welchWsizeSec;
    opt.refractoryMs = refractoryMs; opt.bpmMin = bpmMin; opt.bpmMax = bpmMax;
    opt.highPrecisionFs = highPrecisionFs;
    const char* code = nullptr; std::string msg;
    if (!hp_validate_options(fs, opt, &code, &msg)) {
        if (code) return env->NewStringUTF(code);
        return env->NewStringUTF("HEARTPY_E015");
    }
    return nullptr;
}

// ------------------------------
// Android JSI install + host functions
// ------------------------------

// Forward declare installer
static void installBinding(facebook::jsi::Runtime& rt);

extern "C" JNIEXPORT void JNICALL
Java_com_heartpy_HeartPyModule_installJSIHybrid(JNIEnv*, jclass, jlong runtimePtr) {
    if (runtimePtr == 0) return;
    auto* runtime = reinterpret_cast<facebook::jsi::Runtime*>(runtimePtr);
    installBinding(*runtime);
}

// Handle registry for JSI path (32-bit IDs)
static std::unordered_map<uint32_t, void*> g_handles;
static std::mutex g_handles_m;
static std::atomic<uint32_t> g_next_id{1};

static uint32_t hp_handle_register(void* p) {
    std::lock_guard<std::mutex> lock(g_handles_m);
    uint32_t id = g_next_id.fetch_add(1);
    g_handles[id] = p;
    return id;
}
static void* hp_handle_get(uint32_t id) {
    std::lock_guard<std::mutex> lock(g_handles_m);
    auto it = g_handles.find(id);
    return (it == g_handles.end() ? nullptr : it->second);
}
static void hp_handle_remove(uint32_t id) {
    std::lock_guard<std::mutex> lock(g_handles_m);
    auto it = g_handles.find(id);
    if (it != g_handles.end()) {
        void* p = it->second;
        g_handles.erase(it);
        hp_rt_destroy(p);
    }
}

// Zero-copy flag (updated from Java setConfig)
static std::atomic<bool> g_zero_copy_enabled{true};
static std::atomic<unsigned long long> g_zero_copy_used{0};
static std::atomic<unsigned long long> g_fallback_copy_used{0};

extern "C" JNIEXPORT void JNICALL
Java_com_heartpy_HeartPyModule_setZeroCopyEnabledNative(JNIEnv*, jclass, jboolean enabled) {
    g_zero_copy_enabled.store(enabled == JNI_TRUE);
}

extern "C" JNIEXPORT jlongArray JNICALL
Java_com_heartpy_HeartPyModule_getJSIStatsNative(JNIEnv* env, jclass) {
    jlongArray arr = env->NewLongArray(2);
    jlong vals[2];
    vals[0] = (jlong)g_zero_copy_used.load();
    vals[1] = (jlong)g_fallback_copy_used.load();
    env->SetLongArrayRegion(arr, 0, 2, vals);
    return arr;
}

static void installBinding(facebook::jsi::Runtime& rt) {
    using namespace facebook::jsi;
    // __hpRtCreate(fs:number, options?:object) -> number (id)
    auto fnCreate = Function::createFromHostFunction(
        rt,
        PropNameID::forAscii(rt, "__hpRtCreate"),
        2,
        [](Runtime& rt, const Value&, const Value* args, size_t count) -> Value {
            if (count < 1 || !args[0].isNumber()) {
                throw JSError(rt, "HEARTPY_E001: invalid fs");
            }
            double fs = args[0].asNumber();
            heartpy::Options opt;
            if (count > 1 && args[1].isObject()) {
                opt = hp_build_options_from_jsi(rt, args[1].asObject(rt), nullptr, nullptr);
            }
            const char* code = nullptr; std::string msg;
            if (!hp_validate_options(fs, opt, &code, &msg)) {
                std::string m = (code ? code : "HEARTPY_E015"); m += ": "; m += msg;
                throw JSError(rt, m.c_str());
            }
            void* p = hp_rt_create(fs, &opt);
            if (!p) throw JSError(rt, "HEARTPY_E004: create failed");
            uint32_t id = hp_handle_register(p);
            return Value((double)id);
        }
    );
    rt.global().setProperty(rt, "__hpRtCreate", fnCreate);

    // __hpRtSetWindow(handle:number, windowSeconds:number) -> void
    auto fnSetWindow = Function::createFromHostFunction(
        rt,
        PropNameID::forAscii(rt, "__hpRtSetWindow"),
        2,
        [](Runtime& rt, const Value&, const Value* args, size_t count) -> Value {
            if (count < 2 || !args[0].isNumber() || !args[1].isNumber()) {
                throw JSError(rt, "HEARTPY_E201: invalid arguments for setWindow");
            }
            double h = args[0].asNumber();
            double windowSec = args[1].asNumber();
            if (!(windowSec > 0.0)) {
                throw JSError(rt, "HEARTPY_E201: windowSeconds must be > 0");
            }
            void* ptr = hp_handle_get((uint32_t)h);
            if (!ptr) {
                throw JSError(rt, "HEARTPY_E101: invalid or destroyed handle");
            }
            hp_rt_set_window(ptr, windowSec);
            return Value::undefined();
        }
    );
    rt.global().setProperty(rt, "__hpRtSetWindow", fnSetWindow);

    // __hpRtPush(handle:number, data:Float32Array, t0?:number) -> void
    auto fnPush = Function::createFromHostFunction(
        rt,
        PropNameID::forAscii(rt, "__hpRtPush"),
        3,
        [](Runtime& rt, const Value&, const Value* args, size_t count) -> Value {
            if (count < 2) throw JSError(rt, "HEARTPY_E102: missing data");
            if (!args[0].isNumber()) throw JSError(rt, "HEARTPY_E101: invalid handle");
            uint32_t id = (uint32_t)args[0].asNumber();
            void* p = hp_handle_get(id);
            if (!p) throw JSError(rt, "HEARTPY_E101: invalid handle");
            auto arr = args[1];
            if (!arr.isObject()) throw JSError(rt, "HEARTPY_E102: invalid buffer");
            auto o = arr.asObject(rt);
            size_t len = (size_t)o.getProperty(rt, "length").asNumber();
            if (len == 0) throw JSError(rt, "HEARTPY_E102: empty buffer");
            double t0 = (count > 2 && args[2].isNumber()) ? args[2].asNumber() : 0.0;
            const size_t MAX_SAMPLES_PER_PUSH = 5000;
            if (len > MAX_SAMPLES_PER_PUSH) throw JSError(rt, "HEARTPY_E102: buffer too large");

            bool usedZeroCopy = false;
            if (g_zero_copy_enabled.load()) {
                try {
                    size_t bpe = (size_t)o.getProperty(rt, "BYTES_PER_ELEMENT").asNumber();
                    size_t byteOffset = (size_t)o.getProperty(rt, "byteOffset").asNumber();
                    auto buf = o.getProperty(rt, "buffer").asObject(rt);
                    auto ab = buf.getArrayBuffer(rt);
                    uint8_t* base = ab.data(rt);
                    size_t abSize = ab.size(rt);
                    size_t need = byteOffset + len * bpe;
                    if (bpe == 4 && base && need <= abSize && (byteOffset % 4 == 0)) {
                        float* data = reinterpret_cast<float*>(base + byteOffset);
                        hp_rt_push(p, data, len, t0);
                        usedZeroCopy = true;
                        g_zero_copy_used.fetch_add(1);
                        __android_log_print(ANDROID_LOG_DEBUG, "HeartPyJSI", "rtPush: zero-copy used (len=%zu)", len);
                    }
                } catch (...) {
                    // fall through to copy path
                }
            }
            if (!usedZeroCopy) {
                __android_log_print(ANDROID_LOG_DEBUG, "HeartPyJSI", "rtPush: fallback copy path (len=%zu)", len);
                std::vector<float> tmp; tmp.reserve(len);
                for (size_t i = 0; i < len; ++i) tmp.push_back((float)o.getPropertyAtIndex(rt, (uint32_t)i).asNumber());
                hp_rt_push(p, tmp.data(), tmp.size(), t0);
                g_fallback_copy_used.fetch_add(1);
            }
            return Value::undefined();
        }
    );
    rt.global().setProperty(rt, "__hpRtPush", fnPush);

    // __hpRtPushTs(handle:number, samples:Float32Array, timestamps:Float64Array) -> void
    auto fnPushTs = Function::createFromHostFunction(
        rt,
        PropNameID::forAscii(rt, "__hpRtPushTs"),
        3,
        [](Runtime& rt, const Value&, const Value* args, size_t count) -> Value {
            if (count < 3) throw JSError(rt, "HEARTPY_E102: missing buffers");
            if (!args[0].isNumber()) throw JSError(rt, "HEARTPY_E101: invalid handle");
            uint32_t id = (uint32_t)args[0].asNumber();
            void* p = hp_handle_get(id);
            if (!p) throw JSError(rt, "HEARTPY_E101: invalid handle");

            auto samplesVal = args[1];
            auto timestampsVal = args[2];
            if (!samplesVal.isObject() || !timestampsVal.isObject()) {
                throw JSError(rt, "HEARTPY_E102: invalid buffers");
            }

            auto samplesObj = samplesVal.asObject(rt);
            auto timestampsObj = timestampsVal.asObject(rt);
            size_t len = (size_t)samplesObj.getProperty(rt, "length").asNumber();
            size_t lenTs = (size_t)timestampsObj.getProperty(rt, "length").asNumber();
            if (len == 0 || lenTs == 0) throw JSError(rt, "HEARTPY_E102: empty buffer");
            size_t countEffective = std::min(len, lenTs);
            const size_t MAX_SAMPLES_PER_PUSH = 5000;
            if (countEffective > MAX_SAMPLES_PER_PUSH) throw JSError(rt, "HEARTPY_E102: buffer too large");

            std::vector<float> samples;
            samples.reserve(countEffective);
            for (size_t i = 0; i < countEffective; ++i) {
                samples.push_back((float)samplesObj.getPropertyAtIndex(rt, (uint32_t)i).asNumber());
            }

            std::vector<double> timestamps;
            timestamps.reserve(countEffective);
            for (size_t i = 0; i < countEffective; ++i) {
                timestamps.push_back(timestampsObj.getPropertyAtIndex(rt, (uint32_t)i).asNumber());
            }

            hp_rt_push_ts(p, samples.data(), timestamps.data(), countEffective);
            return Value::undefined();
        }
    );
    rt.global().setProperty(rt, "__hpRtPushTs", fnPushTs);

    // __hpRtPoll(handle:number) -> object | null
    auto fnPoll = Function::createFromHostFunction(
        rt,
        PropNameID::forAscii(rt, "__hpRtPoll"),
        1,
        [](Runtime& rt, const Value&, const Value* args, size_t count) -> Value {
            if (count < 1 || !args[0].isNumber()) throw JSError(rt, "HEARTPY_E111: invalid handle");
            uint32_t id = (uint32_t)args[0].asNumber();
            void* p = hp_handle_get(id);
            if (!p) throw JSError(rt, "HEARTPY_E111: invalid handle");
            heartpy::HeartMetrics out;
            if (!hp_rt_poll(p, &out)) return Value::null();
            Object obj(rt);
            obj.setProperty(rt, "bpm", out.bpm);
            // rrList
            {
                Array rr(rt, out.rrList.size());
                for (size_t i=0;i<out.rrList.size();++i) rr.setValueAtIndex(rt, i, out.rrList[i]);
                obj.setProperty(rt, "rrList", rr);
            }
            // quality
            {
                Object q(rt);
                q.setProperty(rt, "snrDb", out.quality.snrDb);
                q.setProperty(rt, "confidence", out.quality.confidence);
                obj.setProperty(rt, "quality", q);
            }
            return obj;
        }
    );
    rt.global().setProperty(rt, "__hpRtPoll", fnPoll);

    // __hpRtDestroy(handle:number)
    auto fnDestroy = Function::createFromHostFunction(
        rt,
        PropNameID::forAscii(rt, "__hpRtDestroy"),
        1,
        [](Runtime& rt, const Value&, const Value* args, size_t count) -> Value {
            if (count < 1 || !args[0].isNumber()) throw JSError(rt, "HEARTPY_E121: invalid handle");
            uint32_t id = (uint32_t)args[0].asNumber();
            hp_handle_remove(id);
            return Value::undefined();
        }
    );
    rt.global().setProperty(rt, "__hpRtDestroy", fnDestroy);
}
