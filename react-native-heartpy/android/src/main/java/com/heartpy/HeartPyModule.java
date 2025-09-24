package com.heartpy;

import androidx.annotation.NonNull;

import com.facebook.react.bridge.Arguments;
import com.facebook.react.bridge.Promise;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.ReactContext;
import com.facebook.react.bridge.ReactMethod;
import com.facebook.react.bridge.ReadableArray;
import com.facebook.react.bridge.ReadableMap;
import com.facebook.react.bridge.WritableArray;
import android.util.Log;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.ConcurrentLinkedQueue;

import com.facebook.fbreact.specs.NativeHeartPySpec;
import javax.annotation.Nullable;

public class HeartPyModule extends NativeHeartPySpec {
    public static final String NAME = NativeHeartPySpec.NAME;
    static {
        System.loadLibrary("heartpy_rn");
    }
    private static native String analyzeNativeJson(
            double[] signal, double fs,
            double lowHz, double highHz, int order,
            int nfft, double overlap, double welchWsizeSec,
            double refractoryMs, double thresholdScale, double bpmMin, double bpmMax,
            boolean interpClipping, double clippingThreshold,
            boolean hampelCorrect, int hampelWindow, double hampelThreshold,
            boolean removeBaselineWander, boolean enhancePeaks,
            boolean highPrecision, double highPrecisionFs,
            boolean rejectSegmentwise, double segmentRejectThreshold, int segmentRejectMaxRejects, int segmentRejectWindowBeats, double segmentRejectOverlap,
            boolean cleanRR, int cleanMethod,
            double segmentWidth, double segmentOverlap, double segmentMinSize, boolean replaceOutliers,
            double rrSplineS, double rrSplineTargetSse, double rrSplineSmooth,
            boolean breathingAsBpm,
            int sdsdMode,
            int poincareMode,
            boolean pnnAsPercent,
            double snrTauSec, double snrActiveTauSec,
            boolean adaptivePsd,
            boolean thresholdRR,
            boolean calcFreq,
            int filterMode
    );

    private static native String analyzeRRNativeJson(
            double[] rr,
            boolean cleanRR, int cleanMethod,
            boolean breathingAsBpm,
            boolean thresholdRR,
            int sdsdMode,
            int poincareMode,
            boolean pnnAsPercent
    );

    private static native String analyzeSegmentwiseNativeJson(
            double[] signal, double fs,
            double lowHz, double highHz, int order,
            int nfft, double overlap, double welchWsizeSec,
            double refractoryMs, double thresholdScale, double bpmMin, double bpmMax,
            boolean interpClipping, double clippingThreshold,
            boolean hampelCorrect, int hampelWindow, double hampelThreshold,
            boolean removeBaselineWander, boolean enhancePeaks,
            boolean highPrecision, double highPrecisionFs,
            boolean rejectSegmentwise, double segmentRejectThreshold, int segmentRejectMaxRejects, int segmentRejectWindowBeats, double segmentRejectOverlap,
            boolean cleanRR, int cleanMethod,
            double segmentWidth, double segmentOverlap, double segmentMinSize, boolean replaceOutliers,
            double rrSplineS, double rrSplineTargetSse, double rrSplineSmooth,
            boolean breathingAsBpm,
            int sdsdMode,
            int poincareMode,
            boolean pnnAsPercent,
            double snrTauSec, double snrActiveTauSec,
            boolean thresholdRR,
            boolean calcFreq,
            int filterMode
    );

    private static native double[] interpolateClippingNative(double[] signal, double fs, double threshold);
    private static native double[] hampelFilterNative(double[] signal, int windowSize, double threshold);
    private static native double[] scaleDataNative(double[] signal, double newMin, double newMax);

    // Realtime streaming native bindings (P0)
    private static native long rtCreateNative(
            double fs,
            double lowHz, double highHz, int order,
            int nfft, double overlap, double welchWsizeSec,
            double refractoryMs, double thresholdScale, double bpmMin, double bpmMax,
            boolean interpClipping, double clippingThreshold,
            boolean hampelCorrect, int hampelWindow, double hampelThreshold,
            boolean removeBaselineWander, boolean enhancePeaks,
            boolean highPrecision, double highPrecisionFs,
            boolean rejectSegmentwise, double segmentRejectThreshold, int segmentRejectMaxRejects, int segmentRejectWindowBeats, double segmentRejectOverlap,
            boolean cleanRR, int cleanMethod,
            double segmentWidth, double segmentOverlap, double segmentMinSize, boolean replaceOutliers,
            double rrSplineS, double rrSplineTargetSse, double rrSplineSmooth,
            boolean breathingAsBpm,
            int sdsdMode,
            int poincareMode,
            boolean pnnAsPercent,
            double snrTauSec, double snrActiveTauSec,
            boolean thresholdRR,
            boolean calcFreq,
            int filterMode
    );
    private static native void rtSetWindowNative(long handle, double windowSeconds);
    private static native void rtPushNative(long handle, double[] samples, double t0);
    private static native void rtPushTsNative(long handle, double[] samples, double[] timestamps);
    private static native String rtPollNative(long handle);
    private static native void rtDestroyNative(long handle);
    private static native String rtValidateOptionsNative(double fs,
                                                         double lowHz, double highHz,
                                                         int order,
                                                         int nfft,
                                                         double overlap,
                                                         double welchWsizeSec,
                                                         double refractoryMs,
                                                         double bpmMin, double bpmMax,
                                                         double highPrecisionFs);
    private static native void installJSIHybrid(long runtimePtr);
    private static native void setZeroCopyEnabledNative(boolean enabled);
    private static native long[] getJSIStatsNative();
    private static native HeartMetricsTyped analyzeNativeTyped(
            double[] signal, double fs,
            double lowHz, double highHz, int order,
            int nfft, double overlap, double welchWsizeSec,
            double refractoryMs, double thresholdScale, double bpmMin, double bpmMax,
            boolean interpClipping, double clippingThreshold,
            boolean hampelCorrect, int hampelWindow, double hampelThreshold,
            boolean removeBaselineWander, boolean enhancePeaks,
            boolean highPrecision, double highPrecisionFs,
            boolean rejectSegmentwise, double segmentRejectThreshold, int segmentRejectMaxRejects, int segmentRejectWindowBeats, double segmentRejectOverlap,
            boolean cleanRR, int cleanMethod,
            double segmentWidth, double segmentOverlap, double segmentMinSize, boolean replaceOutliers,
            double rrSplineS, double rrSplineTargetSse, double rrSplineSmooth,
            boolean breathingAsBpm,
            int sdsdMode,
            int poincareMode,
            boolean pnnAsPercent,
            double snrTauSec, double snrActiveTauSec,
            boolean adaptivePsd,
            boolean thresholdRR,
            boolean calcFreq,
            int filterMode
    );

    private static native HeartMetricsTyped analyzeSegmentwiseNativeTyped(
            double[] signal, double fs,
            double lowHz, double highHz, int order,
            int nfft, double overlap, double welchWsizeSec,
            double refractoryMs, double thresholdScale, double bpmMin, double bpmMax,
            boolean interpClipping, double clippingThreshold,
            boolean hampelCorrect, int hampelWindow, double hampelThreshold,
            boolean removeBaselineWander, boolean enhancePeaks,
            boolean highPrecision, double highPrecisionFs,
            boolean rejectSegmentwise, double segmentRejectThreshold, int segmentRejectMaxRejects, int segmentRejectWindowBeats, double segmentRejectOverlap,
            boolean cleanRR, int cleanMethod,
            double segmentWidth, double segmentOverlap, double segmentMinSize, boolean replaceOutliers,
            double rrSplineS, double rrSplineTargetSse, double rrSplineSmooth,
            boolean breathingAsBpm,
            int sdsdMode,
            int poincareMode,
            boolean pnnAsPercent,
            double snrTauSec, double snrActiveTauSec,
            boolean adaptivePsd,
            boolean thresholdRR,
            boolean calcFreq,
            int filterMode
    );

    private static native HeartMetricsTyped analyzeRRNativeTyped(
            double[] rr,
            boolean cleanRR, int cleanMethod,
            boolean breathingAsBpm,
            boolean thresholdRR,
            int sdsdMode,
            int poincareMode,
            boolean pnnAsPercent
    );

    static final class QualityTyped {
        QualityTyped() {}
        double totalBeats;
        double rejectedBeats;
        double rejectionRate;
        boolean goodQuality;
        double snrDb;
        double confidence;
        double f0Hz;
        double maPercActive;
        double doublingFlag;
        double softDoublingFlag;
        double doublingHintFlag;
        double hardFallbackActive;
        double rrFallbackModeActive;
        double snrWarmupActive;
        double snrSampleCount;
        double refractoryMsActive;
        double minRRBoundMs;
        double pairFrac;
        double rrShortFrac;
        double rrLongMs;
        double pHalfOverFund;
        String qualityWarning;
    }

    static final class BinarySegmentTyped {
        BinarySegmentTyped() {}
        int index;
        int startBeat;
        int endBeat;
        int totalBeats;
        int rejectedBeats;
        boolean accepted;
    }

    static final class HeartMetricsTyped {
        HeartMetricsTyped() {}
        double bpm;
        double sdnn;
        double rmssd;
        double sdsd;
        double pnn20;
        double pnn50;
        double nn20;
        double nn50;
        double mad;
        double sd1;
        double sd2;
        double sd1sd2Ratio;
        double ellipseArea;
        double vlf;
        double lf;
        double hf;
        double lfhf;
        double totalPower;
        double lfNorm;
        double hfNorm;
        double breathingRate;
        double[] ibiMs;
        double[] rrList;
        int[] peakList;
        int[] peakListRaw;
        int[] binaryPeakMask;
        double[] peakTimestamps;
        double[] waveform_values;
        double[] waveform_timestamps;
        BinarySegmentTyped[] binarySegments;
        QualityTyped quality;
    }

    // ---------- Step 0: Risk mitigation flags & profiling ----------
    private static volatile boolean CFG_JSI_ENABLED = true;
    private static volatile boolean CFG_ZERO_COPY_ENABLED = true; // honored in JSI step
    private static volatile boolean CFG_DEBUG = false;
    private static final int MAX_SAMPLES_PER_PUSH = 5000;

    private static double[] readableArrayToDoubleArray(ReadableArray array) {
        if (array == null) {
            return new double[0];
        }
        final int size = array.size();
        double[] out = new double[size];
        for (int i = 0; i < size; i++) {
            out[i] = array.getDouble(i);
        }
        return out;
    }

    @Override
    @ReactMethod(isBlockingSynchronousMethod = true)
    public com.facebook.react.bridge.WritableMap analyze(ReadableArray signal, double fs, @Nullable ReadableMap options) {
        return analyzeInternal(readableArrayToDoubleArray(signal), fs, options);
    }

    @Override
    @ReactMethod(isBlockingSynchronousMethod = true)
    public com.facebook.react.bridge.WritableMap analyzeSegmentwise(ReadableArray signal, double fs, @Nullable ReadableMap options) {
        return analyzeSegmentwiseInternal(readableArrayToDoubleArray(signal), fs, options);
    }

    @Override
    @ReactMethod(isBlockingSynchronousMethod = true)
    public com.facebook.react.bridge.WritableMap analyzeRR(ReadableArray rrIntervals, @Nullable ReadableMap options) {
        return analyzeRRInternal(readableArrayToDoubleArray(rrIntervals), options);
    }

    @Override
    @ReactMethod
    public void analyzeAsync(ReadableArray signal, double fs, @Nullable ReadableMap options, Promise promise) {
        analyzeAsyncInternal(readableArrayToDoubleArray(signal), fs, options, promise);
    }

    @Override
    @ReactMethod
    public void analyzeSegmentwiseAsync(ReadableArray signal, double fs, @Nullable ReadableMap options, Promise promise) {
        analyzeSegmentwiseAsyncInternal(readableArrayToDoubleArray(signal), fs, options, promise);
    }

    @Override
    @ReactMethod
    public void analyzeRRAsync(ReadableArray rrIntervals, @Nullable ReadableMap options, Promise promise) {
        analyzeRRAsyncInternal(readableArrayToDoubleArray(rrIntervals), options, promise);
    }

    @Override
    @ReactMethod(isBlockingSynchronousMethod = true)
    public com.facebook.react.bridge.WritableMap analyzeTyped(ReadableArray signal, double fs, @Nullable ReadableMap options) {
        return analyzeTypedInternal(readableArrayToDoubleArray(signal), fs, options);
    }

    @Override
    @ReactMethod(isBlockingSynchronousMethod = true)
    public com.facebook.react.bridge.WritableMap analyzeSegmentwiseTyped(ReadableArray signal, double fs, @Nullable ReadableMap options) {
        return analyzeSegmentwiseTypedInternal(readableArrayToDoubleArray(signal), fs, options);
    }

    @Override
    @ReactMethod(isBlockingSynchronousMethod = true)
    public com.facebook.react.bridge.WritableMap analyzeRRTyped(ReadableArray rrIntervals, @Nullable ReadableMap options) {
        return analyzeRRTypedInternal(readableArrayToDoubleArray(rrIntervals), options);
    }

    @Override
    @ReactMethod
    public void analyzeAsyncTyped(ReadableArray signal, double fs, @Nullable ReadableMap options, Promise promise) {
        analyzeAsyncTypedInternal(readableArrayToDoubleArray(signal), fs, options, promise);
    }

    @Override
    @ReactMethod
    public void analyzeSegmentwiseAsyncTyped(ReadableArray signal, double fs, @Nullable ReadableMap options, Promise promise) {
        analyzeSegmentwiseAsyncTypedInternal(readableArrayToDoubleArray(signal), fs, options, promise);
    }

    @Override
    @ReactMethod
    public void analyzeRRAsyncTyped(ReadableArray rrIntervals, @Nullable ReadableMap options, Promise promise) {
        analyzeRRAsyncTypedInternal(readableArrayToDoubleArray(rrIntervals), options, promise);
    }

    @Override
    @ReactMethod(isBlockingSynchronousMethod = true)
    public WritableArray interpolateClipping(ReadableArray signal, double fs, @Nullable Double threshold) {
        return interpolateClippingInternal(readableArrayToDoubleArray(signal), fs, threshold != null ? threshold : 1020.0);
    }

    @Override
    @ReactMethod(isBlockingSynchronousMethod = true)
    public WritableArray hampelFilter(ReadableArray signal, @Nullable Double windowSize, @Nullable Double threshold) {
        int win = windowSize != null ? (int) Math.round(windowSize) : 6;
        double thr = threshold != null ? threshold : 3.0;
        return hampelFilterInternal(readableArrayToDoubleArray(signal), win, thr);
    }

    @Override
    @ReactMethod(isBlockingSynchronousMethod = true)
    public WritableArray scaleData(ReadableArray signal, @Nullable Double newMin, @Nullable Double newMax) {
        double min = newMin != null ? newMin : 0.0;
        double max = newMax != null ? newMax : 1024.0;
        return scaleDataInternal(readableArrayToDoubleArray(signal), min, max);
    }

    @Override
    @ReactMethod
    public void rtCreate(double fs, @Nullable ReadableMap options, Promise promise) {
        rtCreateInternal(fs, options, promise);
    }

    @Override
    @ReactMethod
    public void rtPush(double handle, ReadableArray samples, @Nullable Double t0, Promise promise) {
        rtPushInternal(handle, readableArrayToDoubleArray(samples), t0, promise);
    }

    @Override
    @ReactMethod
    public void rtPushTs(double handle, ReadableArray samples, ReadableArray timestamps, Promise promise) {
        rtPushTsInternal(handle, readableArrayToDoubleArray(samples), readableArrayToDoubleArray(timestamps), promise);
    }

    @Override
    @ReactMethod
    public void rtPoll(double handle, Promise promise) {
        rtPollInternal(handle, promise);
    }

    @Override
    @ReactMethod
    public void rtSetWindow(double handle, double windowSeconds, Promise promise) {
        rtSetWindowInternal(handle, windowSeconds, promise);
    }

    @Override
    @ReactMethod
    public void rtDestroy(double handle, Promise promise) {
        rtDestroyInternal(handle, promise);
    }

    @Override
    @ReactMethod
    public void addListener(String eventType) {
        super.addListener(eventType);
    }

    @Override
    @ReactMethod
    public void removeListeners(double count) {
        super.removeListeners(count);
    }

    private static final AtomicInteger NM_PUSH_SUBMIT = new AtomicInteger(0);
    private static final AtomicInteger NM_PUSH_DONE = new AtomicInteger(0);
    private static final AtomicInteger NM_POLL_SUBMIT = new AtomicInteger(0);
    private static final AtomicInteger NM_POLL_DONE = new AtomicInteger(0);

    @Override
    @ReactMethod(isBlockingSynchronousMethod = true)
    public com.facebook.react.bridge.WritableMap getConfig() {
        com.facebook.react.bridge.WritableMap map = com.facebook.react.bridge.Arguments.createMap();
        map.putBoolean("jsiEnabled", CFG_JSI_ENABLED);
        map.putBoolean("zeroCopyEnabled", CFG_ZERO_COPY_ENABLED);
        map.putBoolean("debug", CFG_DEBUG);
        map.putInt("maxSamplesPerPush", MAX_SAMPLES_PER_PUSH);
        return map;
    }

    @Override
    @ReactMethod
    public void setConfig(@Nullable com.facebook.react.bridge.ReadableMap cfg) {
        if (cfg == null) return;
        try {
            if (cfg.hasKey("jsiEnabled")) CFG_JSI_ENABLED = cfg.getBoolean("jsiEnabled");
            if (cfg.hasKey("zeroCopyEnabled")) {
                CFG_ZERO_COPY_ENABLED = cfg.getBoolean("zeroCopyEnabled");
                try { setZeroCopyEnabledNative(CFG_ZERO_COPY_ENABLED); } catch (Throwable ignore) {}
            }
            if (cfg.hasKey("debug")) CFG_DEBUG = cfg.getBoolean("debug");
            Log.d("HeartPyJSI", "setConfig jsi=" + CFG_JSI_ENABLED + " zeroCopy=" + CFG_ZERO_COPY_ENABLED + " debug=" + CFG_DEBUG);
        } catch (Throwable t) {
            Log.w("HeartPyJSI", "setConfig error: " + t.getMessage());
        }
    }

    public HeartPyModule(@NonNull ReactApplicationContext reactContext) {
        super(reactContext);
    }

    @NonNull
    @Override
    @Override
    public String getName() {
        return "HeartPyModule";
    }

    // Cross-platform PPG buffer (Android parity with iOS notification path)
    private static final ConcurrentLinkedQueue<Double> PPG_BUFFER = new ConcurrentLinkedQueue<>();
    private static final ConcurrentLinkedQueue<Double> PPG_TS_BUFFER = new ConcurrentLinkedQueue<>();
    
    public static void addPPGSample(double value) {
        try {
            if (Double.isNaN(value) || Double.isInfinite(value)) return;
            PPG_BUFFER.add(value);
            // Keep last ~300 samples
            while (PPG_BUFFER.size() > 300) {
                PPG_BUFFER.poll();
            }
        } catch (Throwable ignore) {}
    }
    
    // Timestamp'li PPG sample ekleme metodu (PPGMeanPlugin tarafından kullanılıyor)
    public static void addPPGSampleWithTs(double value, double timestamp) {
        try {
            if (Double.isNaN(value) || Double.isInfinite(value)) return;
            if (Double.isNaN(timestamp) || Double.isInfinite(timestamp)) return;
            PPG_BUFFER.add(value);
            PPG_TS_BUFFER.add(timestamp);
            // Keep last ~300 samples
            while (PPG_BUFFER.size() > 300) {
                PPG_BUFFER.poll();
            }
            while (PPG_TS_BUFFER.size() > 300) {
                PPG_TS_BUFFER.poll();
            }
        } catch (Throwable ignore) {}
    }

    // Last confidence value (0..1)
    private static volatile double LAST_PPG_CONF = 0.0;
    public static void addPPGSampleConfidence(double confidence) {
        try {
            if (Double.isNaN(confidence) || Double.isInfinite(confidence)) return;
            if (confidence < 0.0) confidence = 0.0; if (confidence > 1.0) confidence = 1.0;
            LAST_PPG_CONF = confidence;
        } catch (Throwable ignore) {}
    }

    @ReactMethod
    public void getLatestPPGSamples(Promise promise) {
        final WritableArray out = Arguments.createArray();
        try {
            int drained = 0;
            while (true) {
                final Double v = PPG_BUFFER.poll();
                if (v == null) break;
                out.pushDouble(v);
                drained++;
                if (drained >= 1000) break; // safety cap
            }
            promise.resolve(out);
        } catch (Throwable t) {
            promise.reject("ppg_buffer_error", t);
        }
    }
    
    // Timestamp'li PPG sample'ları okuma (iOS ile uyumluluk için)
    @ReactMethod
    public void getLatestPPGSamplesTs(Promise promise) {
        try {
            final WritableArray samples = Arguments.createArray();
            final WritableArray timestamps = Arguments.createArray();
            int drained = 0;
            
            // Senkronize şekilde oku
            while (true) {
                final Double v = PPG_BUFFER.poll();
                final Double ts = PPG_TS_BUFFER.poll();
                if (v == null || ts == null) break;
                samples.pushDouble(v);
                timestamps.pushDouble(ts);
                drained++;
                if (drained >= 1000) break; // safety cap
            }
            
            // iOS ile aynı format
            final com.facebook.react.bridge.WritableMap result = Arguments.createMap();
            result.putArray("samples", samples);
            result.putArray("timestamps", timestamps);
            promise.resolve(result);
        } catch (Throwable t) {
            promise.reject("ppg_buffer_ts_error", t);
        }
    }

    @ReactMethod
    public void getLastPPGConfidence(Promise promise) {
        try {
            promise.resolve(LAST_PPG_CONF);
        } catch (Throwable t) {
            promise.reject("ppg_conf_error", t);
        }
    }

    // Install Android JSI bindings (blocking, sync)
    @Override
    @ReactMethod(isBlockingSynchronousMethod = true)
    public boolean installJSI() {
        try {
            long ptr = getReactApplicationContext().getJavaScriptContextHolder().get();
            if (ptr == 0) {
                Log.w("HeartPyJSI", "HEARTPY_E901: JS runtime ptr is 0");
                return false;
            }
            installJSIHybrid(ptr);
            Log.d("HeartPyJSI", "installJSIHybrid: success");
            return true;
        } catch (Throwable t) {
            Log.e("HeartPyJSI", "HEARTPY_E900: installJSI failed: " + t.getMessage());
            return false;
        }
    }

    // Debug-only JSI stats: zero-copy vs fallback counts
    @ReactMethod(isBlockingSynchronousMethod = true)
    public com.facebook.react.bridge.WritableMap getJSIStats() {
        com.facebook.react.bridge.WritableMap out = com.facebook.react.bridge.Arguments.createMap();
        try {
            long[] vals = getJSIStatsNative();
            out.putDouble("zeroCopyUsed", (double) (vals != null && vals.length > 0 ? vals[0] : 0));
            out.putDouble("fallbackUsed", (double) (vals != null && vals.length > 1 ? vals[1] : 0));
        } catch (Throwable t) {
            out.putString("error", t.getMessage());
        }
        return out;
    }

    // Single-thread executors per realtime analyzer handle
    private static final java.util.concurrent.ConcurrentHashMap<Long, java.util.concurrent.ExecutorService> EXECUTORS = new java.util.concurrent.ConcurrentHashMap<>();
    private static java.util.concurrent.ExecutorService executorFor(long handle) {
        return EXECUTORS.computeIfAbsent(handle, h -> {
            java.util.concurrent.ExecutorService ex = java.util.concurrent.Executors.newSingleThreadExecutor();
            try { Log.d("HeartPyRT", "executor.create handle="+h+" active="+EXECUTORS.size()); } catch (Throwable t) {}
            return ex;
        });
    }
    private static void shutdownExecutor(long handle) {
        java.util.concurrent.ExecutorService ex = EXECUTORS.remove(handle);
        if (ex != null) {
            ex.shutdownNow();
            try { Log.d("HeartPyRT", "executor.shutdown handle="+handle+" active="+EXECUTORS.size()); } catch (Throwable t) {}
        }
    }

    private static com.facebook.react.bridge.WritableMap jsonToWritableMap(String json) {
        try {
            org.json.JSONObject obj = new org.json.JSONObject(json);
            return toWritableMap(obj);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    private static com.facebook.react.bridge.WritableMap toWritableMap(org.json.JSONObject obj) throws org.json.JSONException {
        com.facebook.react.bridge.WritableMap map = com.facebook.react.bridge.Arguments.createMap();
        java.util.Iterator<String> it = obj.keys();
        while (it.hasNext()) {
            String k = it.next();
            Object v = obj.get(k);
            if (v == org.json.JSONObject.NULL) {
                map.putNull(k);
            } else if (v instanceof org.json.JSONObject) {
                map.putMap(k, toWritableMap((org.json.JSONObject) v));
            } else if (v instanceof org.json.JSONArray) {
                map.putArray(k, toWritableArray((org.json.JSONArray) v));
            } else if (v instanceof Boolean) {
                map.putBoolean(k, (Boolean) v);
            } else if (v instanceof Integer) {
                map.putInt(k, (Integer) v);
            } else if (v instanceof Long) {
                map.putDouble(k, ((Long) v).doubleValue());
            } else if (v instanceof Double) {
                map.putDouble(k, (Double) v);
            } else if (v instanceof String) {
                map.putString(k, (String) v);
            } else {
                map.putString(k, String.valueOf(v));
            }
        }
        return map;
    }

    private static com.facebook.react.bridge.WritableArray toWritableArray(org.json.JSONArray arr) throws org.json.JSONException {
        com.facebook.react.bridge.WritableArray out = com.facebook.react.bridge.Arguments.createArray();
        for (int i = 0; i < arr.length(); i++) {
            Object v = arr.get(i);
            if (v == org.json.JSONObject.NULL) {
                out.pushNull();
            } else if (v instanceof org.json.JSONObject) {
                out.pushMap(toWritableMap((org.json.JSONObject) v));
            } else if (v instanceof org.json.JSONArray) {
                out.pushArray(toWritableArray((org.json.JSONArray) v));
            } else if (v instanceof Boolean) {
                out.pushBoolean((Boolean) v);
            } else if (v instanceof Integer) {
                out.pushInt((Integer) v);
            } else if (v instanceof Long) {
                out.pushDouble(((Long) v).doubleValue());
            } else if (v instanceof Double) {
                out.pushDouble((Double) v);
            } else if (v instanceof String) {
                out.pushString((String) v);
            } else {
                out.pushString(String.valueOf(v));
            }
        }
        return out;
    }

    private static com.facebook.react.bridge.WritableArray doublesToWritable(double[] values) {
        com.facebook.react.bridge.WritableArray out = com.facebook.react.bridge.Arguments.createArray();
        if (values != null) {
            for (double v : values) {
                out.pushDouble(v);
            }
        }
        return out;
    }

    private static com.facebook.react.bridge.WritableArray intsToWritable(int[] values) {
        com.facebook.react.bridge.WritableArray out = com.facebook.react.bridge.Arguments.createArray();
        if (values != null) {
            for (int v : values) {
                out.pushInt(v);
            }
        }
        return out;
    }

    private static com.facebook.react.bridge.WritableArray binarySegmentsToWritable(BinarySegmentTyped[] segments) {
        com.facebook.react.bridge.WritableArray out = com.facebook.react.bridge.Arguments.createArray();
        if (segments != null) {
            for (BinarySegmentTyped seg : segments) {
                if (seg == null) continue;
                com.facebook.react.bridge.WritableMap item = com.facebook.react.bridge.Arguments.createMap();
                item.putInt("index", seg.index);
                item.putInt("startBeat", seg.startBeat);
                item.putInt("endBeat", seg.endBeat);
                item.putInt("totalBeats", seg.totalBeats);
                item.putInt("rejectedBeats", seg.rejectedBeats);
                item.putBoolean("accepted", seg.accepted);
                out.pushMap(item);
            }
        }
        return out;
    }

    private static com.facebook.react.bridge.WritableMap qualityToWritable(QualityTyped quality) {
        com.facebook.react.bridge.WritableMap map = com.facebook.react.bridge.Arguments.createMap();
        if (quality == null) {
            map.putDouble("totalBeats", 0.0);
            map.putDouble("rejectedBeats", 0.0);
            map.putDouble("rejectionRate", 0.0);
            map.putBoolean("goodQuality", true);
            map.putDouble("snrDb", 0.0);
            map.putDouble("confidence", 0.0);
            map.putDouble("f0Hz", 0.0);
            map.putDouble("maPercActive", 0.0);
            map.putDouble("doublingFlag", 0.0);
            map.putDouble("softDoublingFlag", 0.0);
            map.putDouble("doublingHintFlag", 0.0);
            map.putDouble("hardFallbackActive", 0.0);
            map.putDouble("rrFallbackModeActive", 0.0);
            map.putDouble("snrWarmupActive", 0.0);
            map.putDouble("snrSampleCount", 0.0);
            map.putDouble("refractoryMsActive", 0.0);
            map.putDouble("minRRBoundMs", 0.0);
            map.putDouble("pairFrac", 0.0);
            map.putDouble("rrShortFrac", 0.0);
            map.putDouble("rrLongMs", 0.0);
            map.putDouble("pHalfOverFund", 0.0);
            return map;
        }
        map.putDouble("totalBeats", quality.totalBeats);
        map.putDouble("rejectedBeats", quality.rejectedBeats);
        map.putDouble("rejectionRate", quality.rejectionRate);
        map.putBoolean("goodQuality", quality.goodQuality);
        map.putDouble("snrDb", quality.snrDb);
        map.putDouble("confidence", quality.confidence);
        map.putDouble("f0Hz", quality.f0Hz);
        map.putDouble("maPercActive", quality.maPercActive);
        map.putDouble("doublingFlag", quality.doublingFlag);
        map.putDouble("softDoublingFlag", quality.softDoublingFlag);
        map.putDouble("doublingHintFlag", quality.doublingHintFlag);
        map.putDouble("hardFallbackActive", quality.hardFallbackActive);
        map.putDouble("rrFallbackModeActive", quality.rrFallbackModeActive);
        map.putDouble("snrWarmupActive", quality.snrWarmupActive);
        map.putDouble("snrSampleCount", quality.snrSampleCount);
        map.putDouble("refractoryMsActive", quality.refractoryMsActive);
        map.putDouble("minRRBoundMs", quality.minRRBoundMs);
        map.putDouble("pairFrac", quality.pairFrac);
        map.putDouble("rrShortFrac", quality.rrShortFrac);
        map.putDouble("rrLongMs", quality.rrLongMs);
        map.putDouble("pHalfOverFund", quality.pHalfOverFund);
        if (quality.qualityWarning != null && !quality.qualityWarning.isEmpty()) {
            map.putString("qualityWarning", quality.qualityWarning);
        }
        return map;
    }

    private static com.facebook.react.bridge.WritableMap typedToWritableMap(HeartMetricsTyped metrics) {
        if (metrics == null) {
            return com.facebook.react.bridge.Arguments.createMap();
        }
        com.facebook.react.bridge.WritableMap map = com.facebook.react.bridge.Arguments.createMap();
        map.putDouble("bpm", metrics.bpm);
        map.putDouble("sdnn", metrics.sdnn);
        map.putDouble("rmssd", metrics.rmssd);
        map.putDouble("sdsd", metrics.sdsd);
        map.putDouble("pnn20", metrics.pnn20);
        map.putDouble("pnn50", metrics.pnn50);
        map.putDouble("nn20", metrics.nn20);
        map.putDouble("nn50", metrics.nn50);
        map.putDouble("mad", metrics.mad);
        map.putDouble("sd1", metrics.sd1);
        map.putDouble("sd2", metrics.sd2);
        map.putDouble("sd1sd2Ratio", metrics.sd1sd2Ratio);
        map.putDouble("ellipseArea", metrics.ellipseArea);
        map.putDouble("vlf", metrics.vlf);
        map.putDouble("lf", metrics.lf);
        map.putDouble("hf", metrics.hf);
        map.putDouble("lfhf", metrics.lfhf);
        map.putDouble("totalPower", metrics.totalPower);
        map.putDouble("lfNorm", metrics.lfNorm);
        map.putDouble("hfNorm", metrics.hfNorm);
        map.putDouble("breathingRate", metrics.breathingRate);
        map.putArray("ibiMs", doublesToWritable(metrics.ibiMs));
        map.putArray("rrList", doublesToWritable(metrics.rrList));
        map.putArray("peakList", intsToWritable(metrics.peakList));
        map.putArray("peakListRaw", intsToWritable(metrics.peakListRaw));
        map.putArray("binaryPeakMask", intsToWritable(metrics.binaryPeakMask));
        map.putArray("peakTimestamps", doublesToWritable(metrics.peakTimestamps));
        map.putArray("waveform_values", doublesToWritable(metrics.waveform_values));
        map.putArray("waveform_timestamps", doublesToWritable(metrics.waveform_timestamps));
        map.putMap("quality", qualityToWritable(metrics.quality));
        map.putArray("binarySegments", binarySegmentsToWritable(metrics.binarySegments));
        return map;
    }

    private static class Opts {
        double lowHz=0.5, highHz=5.0; int order=2; int nfft=256; double overlap=0.5; double wsizeSec=240.0;
        double refractoryMs=250.0, thresholdScale=0.5, bpmMin=40.0, bpmMax=180.0;
        boolean interpClipping=false; double clippingThreshold=1020.0; boolean hampelCorrect=false; int hampelWindow=6; double hampelThreshold=3.0;
        boolean removeBaselineWander=false, enhancePeaks=false;
        boolean highPrecision=false; double highPrecisionFs=1000.0;
        boolean rejectSegmentwise=false; double segmentRejectThreshold=0.3; int segmentRejectMaxRejects=3; int segmentRejectWindowBeats=10; double segmentRejectOverlap=0.0; boolean cleanRR=false; int cleanMethod=0;
        double segmentWidth=120.0, segmentOverlap=0.0, segmentMinSize=20.0; boolean replaceOutliers=false;
        double rrSplineS=10.0, rrSplineTargetSse=0.0, rrSplineSmooth=0.1;
        boolean breathingAsBpm=false;
        boolean thresholdRR=false;
        int sdsdMode=1; // 1=abs, 0=signed
        int poincareMode=1; // 1=masked, 0=formula
        boolean pnnAsPercent=true;
        double snrTauSec=10.0;
        double snrActiveTauSec=7.0;
        boolean adaptivePsd=true;
        boolean calcFreq=true;
        int filterMode=0; // 0=AUTO,1=RBJ,2=BUTTER
    }

    private static Opts parseOptions(com.facebook.react.bridge.ReadableMap options) {
        Opts o = new Opts();
        if (options == null) return o;
        if (options.hasKey("bandpass")) {
            com.facebook.react.bridge.ReadableMap bp = options.getMap("bandpass");
            if (bp.hasKey("lowHz")) o.lowHz = bp.getDouble("lowHz");
            if (bp.hasKey("highHz")) o.highHz = bp.getDouble("highHz");
            if (bp.hasKey("order")) o.order = bp.getInt("order");
        }
        if (options.hasKey("welch")) {
            com.facebook.react.bridge.ReadableMap w = options.getMap("welch");
            if (w.hasKey("nfft")) o.nfft = w.getInt("nfft");
            if (w.hasKey("overlap")) o.overlap = w.getDouble("overlap");
            if (w.hasKey("wsizeSec")) o.wsizeSec = w.getDouble("wsizeSec");
        }
        if (options.hasKey("peak")) {
            com.facebook.react.bridge.ReadableMap p = options.getMap("peak");
            if (p.hasKey("refractoryMs")) o.refractoryMs = p.getDouble("refractoryMs");
            if (p.hasKey("thresholdScale")) o.thresholdScale = p.getDouble("thresholdScale");
            if (p.hasKey("bpmMin")) o.bpmMin = p.getDouble("bpmMin");
            if (p.hasKey("bpmMax")) o.bpmMax = p.getDouble("bpmMax");
        }
        if (options.hasKey("preprocessing")) {
            com.facebook.react.bridge.ReadableMap prep = options.getMap("preprocessing");
            if (prep.hasKey("interpClipping")) o.interpClipping = prep.getBoolean("interpClipping");
            if (prep.hasKey("clippingThreshold")) o.clippingThreshold = prep.getDouble("clippingThreshold");
            if (prep.hasKey("hampelCorrect")) o.hampelCorrect = prep.getBoolean("hampelCorrect");
            if (prep.hasKey("hampelWindow")) o.hampelWindow = prep.getInt("hampelWindow");
            if (prep.hasKey("hampelThreshold")) o.hampelThreshold = prep.getDouble("hampelThreshold");
            if (prep.hasKey("removeBaselineWander")) o.removeBaselineWander = prep.getBoolean("removeBaselineWander");
            if (prep.hasKey("enhancePeaks")) o.enhancePeaks = prep.getBoolean("enhancePeaks");
        }
        if (options.hasKey("filter")) {
            com.facebook.react.bridge.ReadableMap filt = options.getMap("filter");
            if (filt.hasKey("mode")) {
                String m = filt.getString("mode");
                if ("rbj".equals(m)) o.filterMode = 1;
                else if ("butter".equals(m) || "butter-filtfilt".equals(m)) o.filterMode = 2;
                else o.filterMode = 0;
            }
            if (filt.hasKey("order")) o.order = filt.getInt("order");
        }
        if (options.hasKey("quality")) {
            com.facebook.react.bridge.ReadableMap q = options.getMap("quality");
            if (q.hasKey("rejectSegmentwise")) o.rejectSegmentwise = q.getBoolean("rejectSegmentwise");
            if (q.hasKey("segmentRejectThreshold")) o.segmentRejectThreshold = q.getDouble("segmentRejectThreshold");
            if (q.hasKey("segmentRejectMaxRejects")) o.segmentRejectMaxRejects = q.getInt("segmentRejectMaxRejects");
            if (q.hasKey("cleanRR")) o.cleanRR = q.getBoolean("cleanRR");
            if (q.hasKey("segmentRejectWindowBeats")) o.segmentRejectWindowBeats = q.getInt("segmentRejectWindowBeats");
            if (q.hasKey("segmentRejectOverlap")) o.segmentRejectOverlap = q.getDouble("segmentRejectOverlap");
            if (q.hasKey("cleanMethod")) {
                String m = q.getString("cleanMethod");
                if ("iqr".equals(m)) o.cleanMethod = 1;
                else if ("z-score".equals(m)) o.cleanMethod = 2;
                else o.cleanMethod = 0;
            }
            if (q.hasKey("thresholdRR")) o.thresholdRR = q.getBoolean("thresholdRR");
        }
        if (options.hasKey("timeDomain")) {
            com.facebook.react.bridge.ReadableMap td = options.getMap("timeDomain");
            if (td.hasKey("sdsdMode")) {
                String m = td.getString("sdsdMode");
                o.sdsdMode = ("signed".equals(m) ? 0 : 1);
            }
            if (td.hasKey("pnnAsPercent")) o.pnnAsPercent = td.getBoolean("pnnAsPercent");
        }
        if (options.hasKey("poincare")) {
            com.facebook.react.bridge.ReadableMap pc = options.getMap("poincare");
            if (pc.hasKey("mode")) {
                String m = pc.getString("mode");
                o.poincareMode = ("masked".equals(m) ? 1 : 0);
            }
        }
        if (options.hasKey("highPrecision")) {
            com.facebook.react.bridge.ReadableMap hp = options.getMap("highPrecision");
            if (hp.hasKey("enabled")) o.highPrecision = hp.getBoolean("enabled");
            if (hp.hasKey("targetFs")) o.highPrecisionFs = hp.getDouble("targetFs");
        }
        if (options.hasKey("segmentwise")) {
            com.facebook.react.bridge.ReadableMap seg = options.getMap("segmentwise");
            if (seg.hasKey("width")) o.segmentWidth = seg.getDouble("width");
            if (seg.hasKey("overlap")) o.segmentOverlap = seg.getDouble("overlap");
            if (seg.hasKey("minSize")) o.segmentMinSize = seg.getDouble("minSize");
            if (seg.hasKey("replaceOutliers")) o.replaceOutliers = seg.getBoolean("replaceOutliers");
        }
        if (options.hasKey("rrSpline")) {
            com.facebook.react.bridge.ReadableMap rr = options.getMap("rrSpline");
            if (rr.hasKey("s")) o.rrSplineS = rr.getDouble("s");
            if (rr.hasKey("targetSse")) o.rrSplineTargetSse = rr.getDouble("targetSse");
            if (rr.hasKey("smooth")) o.rrSplineSmooth = rr.getDouble("smooth");
        }
        if (options.hasKey("breathingAsBpm")) o.breathingAsBpm = options.getBoolean("breathingAsBpm");
        if (options.hasKey("calcFreq")) o.calcFreq = options.getBoolean("calcFreq");
        if (options.hasKey("snrTauSec")) o.snrTauSec = options.getDouble("snrTauSec");
        if (options.hasKey("snrActiveTauSec")) o.snrActiveTauSec = options.getDouble("snrActiveTauSec");
        if (options.hasKey("adaptivePsd")) o.adaptivePsd = options.getBoolean("adaptivePsd");
        return o;
    }

    private com.facebook.react.bridge.WritableMap analyzeTypedInternal(double[] signal, double fs,
                                                              com.facebook.react.bridge.ReadableMap options) {
        Opts o = parseOptions(options);
        HeartMetricsTyped metrics = analyzeNativeTyped(signal, fs,
                o.lowHz, o.highHz, o.order,
                o.nfft, o.overlap, o.wsizeSec,
                o.refractoryMs, o.thresholdScale, o.bpmMin, o.bpmMax,
                o.interpClipping, o.clippingThreshold,
                o.hampelCorrect, o.hampelWindow, o.hampelThreshold,
                o.removeBaselineWander, o.enhancePeaks,
                o.highPrecision, o.highPrecisionFs,
                o.rejectSegmentwise, o.segmentRejectThreshold, o.segmentRejectMaxRejects, o.segmentRejectWindowBeats, o.segmentRejectOverlap,
                o.cleanRR, o.cleanMethod,
                o.segmentWidth, o.segmentOverlap, o.segmentMinSize, o.replaceOutliers,
                o.rrSplineS, o.rrSplineTargetSse, o.rrSplineSmooth,
                o.breathingAsBpm,
                o.sdsdMode,
                o.poincareMode,
                o.pnnAsPercent,
                o.snrTauSec, o.snrActiveTauSec,
                o.adaptivePsd,
                o.thresholdRR,
                o.calcFreq,
                o.filterMode);
        return typedToWritableMap(metrics);
    }

    private void analyzeAsyncTypedInternal(double[] signal, double fs,
                                  com.facebook.react.bridge.ReadableMap options,
                                  com.facebook.react.bridge.Promise promise) {
        new Thread(() -> {
            try {
                Opts o = parseOptions(options);
                HeartMetricsTyped metrics = analyzeNativeTyped(signal, fs,
                        o.lowHz, o.highHz, o.order,
                        o.nfft, o.overlap, o.wsizeSec,
                        o.refractoryMs, o.thresholdScale, o.bpmMin, o.bpmMax,
                        o.interpClipping, o.clippingThreshold,
                        o.hampelCorrect, o.hampelWindow, o.hampelThreshold,
                        o.removeBaselineWander, o.enhancePeaks,
                        o.highPrecision, o.highPrecisionFs,
                        o.rejectSegmentwise, o.segmentRejectThreshold, o.segmentRejectMaxRejects, o.segmentRejectWindowBeats, o.segmentRejectOverlap,
                        o.cleanRR, o.cleanMethod,
                        o.segmentWidth, o.segmentOverlap, o.segmentMinSize, o.replaceOutliers,
                        o.rrSplineS, o.rrSplineTargetSse, o.rrSplineSmooth,
                        o.breathingAsBpm,
                        o.sdsdMode,
                        o.poincareMode,
                        o.pnnAsPercent,
                        o.snrTauSec, o.snrActiveTauSec,
                        o.adaptivePsd,
                        o.thresholdRR,
                        o.calcFreq,
                        o.filterMode);
                promise.resolve(typedToWritableMap(metrics));
            } catch (Exception e) {
                promise.reject("analyzeTyped_error", e);
            }
        }).start();
    }

    private com.facebook.react.bridge.WritableMap analyzeSegmentwiseTypedInternal(double[] signal, double fs,
                                                                        com.facebook.react.bridge.ReadableMap options) {
        Opts o = parseOptions(options);
        HeartMetricsTyped metrics = analyzeSegmentwiseNativeTyped(signal, fs,
                o.lowHz, o.highHz, o.order,
                o.nfft, o.overlap, o.wsizeSec,
                o.refractoryMs, o.thresholdScale, o.bpmMin, o.bpmMax,
                o.interpClipping, o.clippingThreshold,
                o.hampelCorrect, o.hampelWindow, o.hampelThreshold,
                o.removeBaselineWander, o.enhancePeaks,
                o.highPrecision, o.highPrecisionFs,
                o.rejectSegmentwise, o.segmentRejectThreshold, o.segmentRejectMaxRejects, o.segmentRejectWindowBeats, o.segmentRejectOverlap,
                o.cleanRR, o.cleanMethod,
                o.segmentWidth, o.segmentOverlap, o.segmentMinSize, o.replaceOutliers,
                o.rrSplineS, o.rrSplineTargetSse, o.rrSplineSmooth,
                o.breathingAsBpm,
                o.sdsdMode,
                o.poincareMode,
                o.pnnAsPercent,
                o.snrTauSec, o.snrActiveTauSec,
                o.adaptivePsd,
                o.thresholdRR,
                o.calcFreq,
                o.filterMode);
        return typedToWritableMap(metrics);
    }

    private void analyzeSegmentwiseAsyncTypedInternal(double[] signal, double fs,
                                             com.facebook.react.bridge.ReadableMap options,
                                             com.facebook.react.bridge.Promise promise) {
        new Thread(() -> {
            try {
                Opts o = parseOptions(options);
                HeartMetricsTyped metrics = analyzeSegmentwiseNativeTyped(signal, fs,
                        o.lowHz, o.highHz, o.order,
                        o.nfft, o.overlap, o.wsizeSec,
                        o.refractoryMs, o.thresholdScale, o.bpmMin, o.bpmMax,
                        o.interpClipping, o.clippingThreshold,
                        o.hampelCorrect, o.hampelWindow, o.hampelThreshold,
                        o.removeBaselineWander, o.enhancePeaks,
                        o.highPrecision, o.highPrecisionFs,
                        o.rejectSegmentwise, o.segmentRejectThreshold, o.segmentRejectMaxRejects, o.segmentRejectWindowBeats, o.segmentRejectOverlap,
                        o.cleanRR, o.cleanMethod,
                        o.segmentWidth, o.segmentOverlap, o.segmentMinSize, o.replaceOutliers,
                        o.rrSplineS, o.rrSplineTargetSse, o.rrSplineSmooth,
                        o.breathingAsBpm,
                        o.sdsdMode,
                        o.poincareMode,
                        o.pnnAsPercent,
                        o.snrTauSec, o.snrActiveTauSec,
                        o.adaptivePsd,
                        o.thresholdRR,
                        o.calcFreq,
                        o.filterMode);
                promise.resolve(typedToWritableMap(metrics));
            } catch (Exception e) {
                promise.reject("analyzeSegmentwiseTyped_error", e);
            }
        }).start();
    }

    private com.facebook.react.bridge.WritableMap analyzeRRTypedInternal(double[] rr,
                                                               com.facebook.react.bridge.ReadableMap options) {
        Opts o = parseOptions(options);
        HeartMetricsTyped metrics = analyzeRRNativeTyped(rr, o.cleanRR, o.cleanMethod, o.breathingAsBpm, o.thresholdRR, o.sdsdMode, o.poincareMode, o.pnnAsPercent);
        return typedToWritableMap(metrics);
    }

    private void analyzeRRAsyncTypedInternal(double[] rr,
                                    com.facebook.react.bridge.ReadableMap options,
                                    com.facebook.react.bridge.Promise promise) {
        new Thread(() -> {
            try {
                Opts o = parseOptions(options);
                HeartMetricsTyped metrics = analyzeRRNativeTyped(rr, o.cleanRR, o.cleanMethod, o.breathingAsBpm, o.thresholdRR, o.sdsdMode, o.poincareMode, o.pnnAsPercent);
                promise.resolve(typedToWritableMap(metrics));
            } catch (Exception e) {
                promise.reject("analyzeRRTyped_error", e);
            }
        }).start();
    }

    private com.facebook.react.bridge.WritableMap analyzeInternal(double[] signal, double fs,
                                                         com.facebook.react.bridge.ReadableMap options) {
        Opts o = parseOptions(options);
        String json = analyzeNativeJson(signal, fs,
                o.lowHz, o.highHz, o.order,
                o.nfft, o.overlap, o.wsizeSec,
                o.refractoryMs, o.thresholdScale, o.bpmMin, o.bpmMax,
                o.interpClipping, o.clippingThreshold,
                o.hampelCorrect, o.hampelWindow, o.hampelThreshold,
                o.removeBaselineWander, o.enhancePeaks,
                o.highPrecision, o.highPrecisionFs,
                o.rejectSegmentwise, o.segmentRejectThreshold, o.segmentRejectMaxRejects, o.segmentRejectWindowBeats, o.segmentRejectOverlap,
                o.cleanRR, o.cleanMethod,
                o.segmentWidth, o.segmentOverlap, o.segmentMinSize, o.replaceOutliers,
                o.rrSplineS, o.rrSplineTargetSse, o.rrSplineSmooth,
                o.breathingAsBpm,
                o.sdsdMode,
                o.poincareMode,
                o.pnnAsPercent,
                o.snrTauSec, o.snrActiveTauSec,
                o.adaptivePsd,
                o.thresholdRR,
                o.calcFreq,
                o.filterMode);
        return jsonToWritableMap(json);
    }

    private void analyzeAsyncInternal(double[] signal, double fs,
                             com.facebook.react.bridge.ReadableMap options,
                             com.facebook.react.bridge.Promise promise) {
        new Thread(() -> {
            try {
                Opts o = parseOptions(options);
                String json = analyzeNativeJson(signal, fs,
                        o.lowHz, o.highHz, o.order,
                        o.nfft, o.overlap, o.wsizeSec,
                        o.refractoryMs, o.thresholdScale, o.bpmMin, o.bpmMax,
                        o.interpClipping, o.clippingThreshold,
                        o.hampelCorrect, o.hampelWindow, o.hampelThreshold,
                        o.removeBaselineWander, o.enhancePeaks,
                        o.highPrecision, o.highPrecisionFs,
                        o.rejectSegmentwise, o.segmentRejectThreshold, o.segmentRejectMaxRejects, o.segmentRejectWindowBeats, o.segmentRejectOverlap,
                        o.cleanRR, o.cleanMethod,
                        o.segmentWidth, o.segmentOverlap, o.segmentMinSize, o.replaceOutliers,
                        o.rrSplineS, o.rrSplineTargetSse, o.rrSplineSmooth,
                        o.breathingAsBpm,
                        o.sdsdMode,
                        o.poincareMode,
                        o.pnnAsPercent,
                        o.snrTauSec, o.snrActiveTauSec,
                        o.adaptivePsd,
                        o.thresholdRR,
                        o.calcFreq,
                        o.filterMode);
                promise.resolve(jsonToWritableMap(json));
            } catch (Exception e) {
                promise.reject("analyze_error", e);
            }
        }).start();
    }

    private com.facebook.react.bridge.WritableMap analyzeRRInternal(double[] rr,
                                                          com.facebook.react.bridge.ReadableMap options) {
        Opts o = parseOptions(options);
        String json = analyzeRRNativeJson(rr, o.cleanRR, o.cleanMethod, o.breathingAsBpm, o.thresholdRR, o.sdsdMode, o.poincareMode, o.pnnAsPercent);
        return jsonToWritableMap(json);
    }

    private void analyzeRRAsyncInternal(double[] rr,
                               com.facebook.react.bridge.ReadableMap options,
                               com.facebook.react.bridge.Promise promise) {
        new Thread(() -> {
            try {
                Opts o = parseOptions(options);
                String json = analyzeRRNativeJson(rr, o.cleanRR, o.cleanMethod, o.breathingAsBpm, o.thresholdRR, o.sdsdMode, o.poincareMode, o.pnnAsPercent);
                promise.resolve(jsonToWritableMap(json));
            } catch (Exception e) {
                promise.reject("analyzeRR_error", e);
            }
        }).start();
    }

    private com.facebook.react.bridge.WritableMap analyzeSegmentwiseInternal(double[] signal, double fs,
                                                                    com.facebook.react.bridge.ReadableMap options) {
        Opts o = parseOptions(options);
        String json = analyzeSegmentwiseNativeJson(signal, fs,
                o.lowHz, o.highHz, o.order,
                o.nfft, o.overlap, o.wsizeSec,
                o.refractoryMs, o.thresholdScale, o.bpmMin, o.bpmMax,
                o.interpClipping, o.clippingThreshold,
                o.hampelCorrect, o.hampelWindow, o.hampelThreshold,
                o.removeBaselineWander, o.enhancePeaks,
                o.highPrecision, o.highPrecisionFs,
                o.rejectSegmentwise, o.segmentRejectThreshold, o.segmentRejectMaxRejects, o.segmentRejectWindowBeats,
                o.segmentRejectOverlap,
                o.cleanRR, o.cleanMethod,
                o.segmentWidth, o.segmentOverlap, o.segmentMinSize, o.replaceOutliers,
                o.rrSplineS, o.rrSplineTargetSse, o.rrSplineSmooth,
                o.breathingAsBpm,
                o.sdsdMode,
                o.poincareMode,
                o.pnnAsPercent,
                o.snrTauSec, o.snrActiveTauSec,
                o.thresholdRR,
                o.calcFreq,
                o.filterMode
        );
        return jsonToWritableMap(json);
    }

    private void analyzeSegmentwiseAsyncInternal(double[] signal, double fs,
                                        com.facebook.react.bridge.ReadableMap options,
                                        com.facebook.react.bridge.Promise promise) {
        new Thread(() -> {
            try {
                Opts o = parseOptions(options);
                String json = analyzeSegmentwiseNativeJson(signal, fs,
                        o.lowHz, o.highHz, o.order,
                        o.nfft, o.overlap, o.wsizeSec,
                        o.refractoryMs, o.thresholdScale, o.bpmMin, o.bpmMax,
                        o.interpClipping, o.clippingThreshold,
                        o.hampelCorrect, o.hampelWindow, o.hampelThreshold,
                        o.removeBaselineWander, o.enhancePeaks,
                        o.highPrecision, o.highPrecisionFs,
                        o.rejectSegmentwise, o.segmentRejectThreshold, o.segmentRejectMaxRejects, o.segmentRejectWindowBeats,
                        o.segmentRejectOverlap,
                        o.cleanRR, o.cleanMethod,
                o.segmentWidth, o.segmentOverlap, o.segmentMinSize, o.replaceOutliers,
                o.rrSplineS, o.rrSplineTargetSse, o.rrSplineSmooth,
                o.breathingAsBpm,
                o.sdsdMode,
                o.poincareMode,
                        o.pnnAsPercent,
                        o.snrTauSec, o.snrActiveTauSec,
                        o.thresholdRR,
                        o.calcFreq,
                        o.filterMode
                );
                promise.resolve(jsonToWritableMap(json));
            } catch (Exception e) {
                promise.reject("analyzeSegmentwise_error", e);
            }
        }).start();
    }

    @ReactMethod(isBlockingSynchronousMethod = true)
    private com.facebook.react.bridge.WritableArray interpolateClippingInternal(double[] signal, double fs, double threshold) {
        double[] y = interpolateClippingNative(signal, fs, threshold);
        com.facebook.react.bridge.WritableArray arr = com.facebook.react.bridge.Arguments.createArray();
        for (double v : y) arr.pushDouble(v);
        return arr;
    }

    @ReactMethod(isBlockingSynchronousMethod = true)
    private com.facebook.react.bridge.WritableArray hampelFilterInternal(double[] signal, int windowSize, double threshold) {
        double[] y = hampelFilterNative(signal, windowSize, threshold);
        com.facebook.react.bridge.WritableArray arr = com.facebook.react.bridge.Arguments.createArray();
        for (double v : y) arr.pushDouble(v);
        return arr;
    }

    @ReactMethod(isBlockingSynchronousMethod = true)
    private com.facebook.react.bridge.WritableArray scaleDataInternal(double[] signal, double newMin, double newMax) {
        double[] y = scaleDataNative(signal, newMin, newMax);
        com.facebook.react.bridge.WritableArray arr = com.facebook.react.bridge.Arguments.createArray();
        for (double v : y) arr.pushDouble(v);
        return arr;
    }

    // ------------------------------
    // Realtime Streaming (NativeModules P0)
    // ------------------------------

    private void rtCreateInternal(double fs, com.facebook.react.bridge.ReadableMap options, Promise promise) {
        try {
            Opts o = parseOptions(options);
            if (fs < 1.0 || fs > 10000.0) { promise.reject("HEARTPY_E001", "Invalid sample rate: " + fs + ". Must be 1-10000 Hz."); return; }
            // Native validation
            String vcode = rtValidateOptionsNative(fs, o.lowHz, o.highHz, o.order, o.nfft, o.overlap, o.wsizeSec, o.refractoryMs, o.bpmMin, o.bpmMax, o.highPrecisionFs);
            if (vcode != null) {
                String msg;
                switch (vcode) {
                    case "HEARTPY_E001": msg = "Invalid sample rate (1-10000 Hz)"; break;
                    case "HEARTPY_E011": msg = "Invalid bandpass (0<=low<high<=fs/2)"; break;
                    case "HEARTPY_E012": msg = "Invalid nfft (64-16384)"; break;
                    case "HEARTPY_E013": msg = "Invalid BPM range (30<=min<max<=240)"; break;
                    case "HEARTPY_E014": msg = "Invalid refractory (50-2000 ms)"; break;
                    default: msg = "Invalid options"; break;
                }
                promise.reject(vcode, msg);
                return;
            }
            long h = rtCreateNative(fs,
                    o.lowHz, o.highHz, o.order,
                    o.nfft, o.overlap, o.wsizeSec,
                    o.refractoryMs, o.thresholdScale, o.bpmMin, o.bpmMax,
                    o.interpClipping, o.clippingThreshold,
                    o.hampelCorrect, o.hampelWindow, o.hampelThreshold,
                    o.removeBaselineWander, o.enhancePeaks,
                    o.highPrecision, o.highPrecisionFs,
                    o.rejectSegmentwise, o.segmentRejectThreshold, o.segmentRejectMaxRejects, o.segmentRejectWindowBeats, o.segmentRejectOverlap,
                    o.cleanRR, o.cleanMethod,
                    o.segmentWidth, o.segmentOverlap, o.segmentMinSize, o.replaceOutliers,
                    o.rrSplineS, o.rrSplineTargetSse, o.rrSplineSmooth,
                    o.breathingAsBpm,
                    o.sdsdMode,
                    o.poincareMode,
                    o.pnnAsPercent,
                    o.snrTauSec, o.snrActiveTauSec,
                    o.thresholdRR,
                    o.calcFreq,
                    o.filterMode);
            if (h == 0) { promise.reject("HEARTPY_E004", "hp_rt_create returned 0"); return; }
            promise.resolve(h);
        } catch (Exception e) {
            promise.reject("HEARTPY_E900", e);
        }
    }

    private void rtSetWindowInternal(double handle, double windowSeconds, Promise promise) {
        try {
            final long h = (long) handle;
            if (h == 0L) { promise.reject("rt_set_window_invalid_args", "Invalid or destroyed handle"); return; }
            if (windowSeconds <= 0.0) { promise.reject("rt_set_window_invalid_args", "windowSeconds must be > 0"); return; }
            rtSetWindowNative(h, windowSeconds);
            promise.resolve(null);
        } catch (Exception e) {
            promise.reject("rt_set_window_error", e);
        }
    }

    private void rtPushInternal(double handle, double[] samples, Double t0, Promise promise) {
        try {
            final long h = (long) handle;
            if (h == 0L) { promise.reject("HEARTPY_E101", "Invalid or destroyed handle"); return; }
            if (samples == null || samples.length == 0) { promise.reject("HEARTPY_E102", "Invalid data buffer: empty buffer"); return; }
            if (samples.length > MAX_SAMPLES_PER_PUSH) { promise.reject("HEARTPY_E102", "Invalid data buffer: too large (max " + MAX_SAMPLES_PER_PUSH + ")"); return; }
            final double ts0 = (t0 == null ? 0.0 : t0.doubleValue());
            executorFor(h).submit(() -> {
                try { rtPushNative(h, samples, ts0); promise.resolve(null); }
                catch (Exception e) { promise.reject("HEARTPY_E900", e); }
                finally { NM_PUSH_DONE.incrementAndGet(); if (CFG_DEBUG) Log.d("HeartPyRT", "nm.push.done="+NM_PUSH_DONE.get()); }
            });
            NM_PUSH_SUBMIT.incrementAndGet(); if (CFG_DEBUG) Log.d("HeartPyRT", "nm.push.submit="+NM_PUSH_SUBMIT.get());
        } catch (Exception e) {
            promise.reject("HEARTPY_E900", e);
        }
    }

    private void rtPollInternal(double handle, Promise promise) {
        try {
            final long h = (long) handle;
            if (h == 0L) { promise.reject("HEARTPY_E111", "Invalid or destroyed handle"); return; }
            executorFor(h).submit(() -> {
                try {
                    String json = rtPollNative(h);
                    if (json == null) { promise.resolve(null); return; }
                    
                    // P1 ENHANCEMENT: Parse JSON and add peakListRaw and windowStartAbs
                    com.facebook.react.bridge.WritableMap result = jsonToWritableMap(json);
                    
                    // Add peakListRaw if available in the JSON response
                    if (result.hasKey("peakListRaw")) {
                        // peakListRaw is already included in the native JSON response
                        // No additional processing needed
                    } else {
                        // Fallback: create empty peakListRaw array
                        com.facebook.react.bridge.WritableArray emptyPeakListRaw = Arguments.createArray();
                        result.putArray("peakListRaw", emptyPeakListRaw);
                    }
                    
                    // P1 FIX: Remove faulty windowStartAbs calculation
                    // The previous heuristic (peakListRaw.size() - 150) was incorrect
                    // For now, set to 0 to indicate early detection phase
                    // In production, the native core should provide the actual window start
                    double windowStartAbs = 0.0; // Default for early detection
                    result.putDouble("windowStartAbs", windowStartAbs);
                    
                    promise.resolve(result);
                } catch (Exception e) {
                    promise.reject("HEARTPY_E900", e);
                }
                finally { NM_POLL_DONE.incrementAndGet(); if (CFG_DEBUG) Log.d("HeartPyRT", "nm.poll.done="+NM_POLL_DONE.get()); }
            });
            NM_POLL_SUBMIT.incrementAndGet(); if (CFG_DEBUG) Log.d("HeartPyRT", "nm.poll.submit="+NM_POLL_SUBMIT.get());
        } catch (Exception e) {
            promise.reject("HEARTPY_E900", e);
        }
    }

    private void rtDestroyInternal(double handle, Promise promise) {
        try {
            final long h = (long) handle;
            if (h == 0L) { promise.resolve(null); return; }
            shutdownExecutor(h);
            rtDestroyNative(h);
            promise.resolve(null);
        } catch (Exception e) {
            promise.reject("rt_destroy_error", e);
        }
    }

    private void rtPushTsInternal(double handle, double[] samples, double[] timestamps, Promise promise) {
        try {
            final long h = (long) handle;
            if (h == 0L) { promise.reject("HEARTPY_E101", "Invalid or destroyed handle"); return; }
            if (samples == null || timestamps == null || samples.length == 0 || timestamps.length == 0) { promise.reject("HEARTPY_E102", "Invalid buffers: empty"); return; }
            final int k = Math.min(samples.length, timestamps.length);
            if (k > MAX_SAMPLES_PER_PUSH) { promise.reject("HEARTPY_E102", "Invalid data buffer: too large (max " + MAX_SAMPLES_PER_PUSH + ")"); return; }
            final double[] xs = (samples.length == k ? samples : java.util.Arrays.copyOf(samples, k));
            final double[] ts = (timestamps.length == k ? timestamps : java.util.Arrays.copyOf(timestamps, k));
            executorFor(h).submit(() -> {
                try { rtPushTsNative(h, xs, ts); promise.resolve(null); }
                catch (Exception e) { promise.reject("HEARTPY_E900", e); }
                finally { NM_PUSH_DONE.incrementAndGet(); if (CFG_DEBUG) Log.d("HeartPyRT", "nm.pushTs.done="+NM_PUSH_DONE.get()); }
            });
            NM_PUSH_SUBMIT.incrementAndGet(); if (CFG_DEBUG) Log.d("HeartPyRT", "nm.pushTs.submit="+NM_PUSH_SUBMIT.get());
        } catch (Exception e) {
            promise.reject("HEARTPY_E900", e);
        }
    }
}
