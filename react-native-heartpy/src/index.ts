export type HeartPyOptions = {
	// Filtering options
	bandpass?: { lowHz: number; highHz: number; order?: number };
	welch?: { nfft?: number; overlap?: number; wsizeSec?: number };
	peak?: { refractoryMs?: number; thresholdScale?: number; bpmMin?: number; bpmMax?: number };
	filter?: { mode?: 'auto' | 'rbj' | 'butter' | 'butter-filtfilt'; order?: number };

	// Global frequency-domain toggle (parity with HeartPy calc_freq)
	calcFreq?: boolean;
	
	// Preprocessing options
	preprocessing?: {
		interpClipping?: boolean;
		clippingThreshold?: number;
		hampelCorrect?: boolean;
		hampelWindow?: number;
		hampelThreshold?: number;
		removeBaselineWander?: boolean;
		enhancePeaks?: boolean;
		scaleData?: boolean;
	};
	
	// Quality and cleaning options
	quality?: {
		rejectSegmentwise?: boolean;
		segmentRejectThreshold?: number;
		segmentRejectMaxRejects?: number;
		segmentRejectWindowBeats?: number;
		segmentRejectOverlap?: number; // 0..1
		cleanRR?: boolean;
		cleanMethod?: 'quotient-filter' | 'iqr' | 'z-score';
		thresholdRR?: boolean; // HeartPy threshold_rr
	};

	// Time-domain controls
	timeDomain?: {
		sdsdMode?: 'signed' | 'abs'; // default 'abs' (HP)
		pnnAsPercent?: boolean; // true: 0..100, false: 0..1 (HP)
	};

	// Poincaré controls
	poincare?: {
		mode?: 'formula' | 'masked'; // default 'masked' (HP)
	};
	
	// High precision mode
	highPrecision?: {
		enabled?: boolean;
		targetFs?: number;
	};

	// RR spline smoothing
	rrSpline?: {
		s?: number; // smoothing factor (lambda)
		targetSse?: number; // Reinsch target SSE
		smooth?: number; // 0..1 pre-smooth blend
	};
	
	// Segmentwise analysis
	segmentwise?: {
		width?: number; // seconds
		overlap?: number; // 0-1
		minSize?: number; // seconds
		replaceOutliers?: boolean;
	};

	// Output controls
	breathingAsBpm?: boolean;

	// Realtime streaming controls
	windowSeconds?: number;

	// Streaming SNR smoothing controls
	snrTauSec?: number;
	snrActiveTauSec?: number;
	adaptivePsd?: boolean;
};

export type QualityInfo = {
	totalBeats: number;
	rejectedBeats: number;
	rejectionRate: number;
	goodQuality: boolean;
	qualityWarning?: string;
	// Streaming quality metrics (from C++ core)
	confidence?: number;
	snrDb?: number;
	f0Hz?: number;
	maPercActive?: number;
	doublingFlag?: boolean;
	softDoublingFlag?: boolean;
	doublingHintFlag?: boolean;
	hardFallbackActive?: boolean;
	rrFallbackModeActive?: boolean;
	refractoryMsActive?: number;
	minRRBoundMs?: number;
	pairFrac?: number;
	rrShortFrac?: number;
	rrLongMs?: number;
	pHalfOverFund?: number;
	snrWarmupActive?: number;
	snrSampleCount?: number;
};

export type HeartPyResult = {
	// Basic metrics
	bpm: number;
	ibiMs: number[];
	rrList: number[];
	peakList: number[];
	peakTimestamps?: number[];
	peakListRaw?: number[];
	binaryPeakMask?: number[];
	waveform_values?: number[];
	waveform_timestamps?: number[];
	
	// Time domain measures
	sdnn: number;
	rmssd: number;
	sdsd: number;
	pnn20: number;
	pnn50: number;
	nn20: number;
	nn50: number;
	mad: number;
	
	// Poincaré analysis
	sd1: number;
	sd2: number;
	sd1sd2Ratio: number;
	ellipseArea: number;
	
	// Frequency domain
	vlf: number;
	lf: number;
	hf: number;
	lfhf: number;
	totalPower: number;
	lfNorm: number;
	hfNorm: number;
	
	// Breathing analysis
	breathingRate: number;
	
	// Quality metrics
	quality: QualityInfo;
	
	// Segmentwise results (if applicable)
	segments?: HeartPyResult[];
};

export function analyze(signal: number[] | Float64Array, fs: number, options?: HeartPyOptions): HeartPyResult {
	const { NativeModules } = require('react-native');
	const Native: any = NativeModules?.HeartPyModule;
	if (!Native?.analyze) throw new Error('HeartPyModule.analyze not available');
	const arr = (signal instanceof Float64Array ? Array.from(signal) : signal) as number[];
	return Native.analyze(arr, fs, options ?? {});
}

export function analyzeSegmentwise(signal: number[] | Float64Array, fs: number, options?: HeartPyOptions): HeartPyResult {
	const { NativeModules } = require('react-native');
	const Native: any = NativeModules?.HeartPyModule;
	if (!Native?.analyzeSegmentwise) throw new Error('HeartPyModule.analyzeSegmentwise not available');
	const arr = (signal instanceof Float64Array ? Array.from(signal) : signal) as number[];
	return Native.analyzeSegmentwise(arr, fs, options ?? {});
}

export function analyzeRR(rrIntervals: number[], options?: HeartPyOptions): HeartPyResult {
	const { NativeModules } = require('react-native');
	const Native: any = NativeModules?.HeartPyModule;
	if (!Native?.analyzeRR) throw new Error('HeartPyModule.analyzeRR not available');
	return Native.analyzeRR(rrIntervals, options ?? {});
}

// Preprocessing functions
export function interpolateClipping(signal: number[], fs: number, threshold: number = 1020): number[] {
	const { NativeModules } = require('react-native');
	const Native: any = NativeModules?.HeartPyModule;
	if (!Native?.interpolateClipping) throw new Error('HeartPyModule.interpolateClipping not available');
	return Native.interpolateClipping(signal, fs, threshold);
}

export function hampelFilter(signal: number[], windowSize: number = 6, threshold: number = 3.0): number[] {
	const { NativeModules } = require('react-native');
	const Native: any = NativeModules?.HeartPyModule;
	if (!Native?.hampelFilter) throw new Error('HeartPyModule.hampelFilter not available');
	return Native.hampelFilter(signal, windowSize, threshold);
}

export function scaleData(signal: number[], newMin: number = 0, newMax: number = 1024): number[] {
	const { NativeModules } = require('react-native');
	const Native: any = NativeModules?.HeartPyModule;
	if (!Native?.scaleData) throw new Error('HeartPyModule.scaleData not available');
	return Native.scaleData(signal, newMin, newMax);
}


// Async variants: avoid blocking the JS thread
export async function analyzeAsync(signal: number[] | Float64Array, fs: number, options?: HeartPyOptions): Promise<HeartPyResult> {
	const { NativeModules } = require('react-native');
	const Native: any = NativeModules?.HeartPyModule;
	if (!Native?.analyzeAsync) throw new Error('HeartPyModule.analyzeAsync not available');
	const arr = (signal instanceof Float64Array ? Array.from(signal) : signal) as number[];
	return Native.analyzeAsync(arr, fs, options ?? {});
}

export function analyzeTyped(signal: number[] | Float64Array, fs: number, options?: HeartPyOptions): HeartPyResult {
	const {NativeModules} = require('react-native');
	const Native: any = NativeModules?.HeartPyModule;
	if (!Native?.analyzeTyped) {
		throw new Error('HeartPyModule.analyzeTyped not available');
	}
	const arr = (signal instanceof Float64Array ? Array.from(signal) : signal) as number[];
	return Native.analyzeTyped(arr, fs, options ?? {});
}

export async function analyzeAsyncTyped(
	signal: number[] | Float64Array,
	fs: number,
	options?: HeartPyOptions,
): Promise<HeartPyResult> {
	const {NativeModules} = require('react-native');
	const Native: any = NativeModules?.HeartPyModule;
	if (!Native?.analyzeAsyncTyped) {
		throw new Error('HeartPyModule.analyzeAsyncTyped not available');
	}
	const arr = (signal instanceof Float64Array ? Array.from(signal) : signal) as number[];
	return Native.analyzeAsyncTyped(arr, fs, options ?? {});
}

export async function analyzeSegmentwiseAsync(signal: number[] | Float64Array, fs: number, options?: HeartPyOptions): Promise<HeartPyResult> {
	const { NativeModules } = require('react-native');
	const Native: any = NativeModules?.HeartPyModule;
	if (!Native?.analyzeSegmentwiseAsync) throw new Error('HeartPyModule.analyzeSegmentwiseAsync not available');
	const arr = (signal instanceof Float64Array ? Array.from(signal) : signal) as number[];
	return Native.analyzeSegmentwiseAsync(arr, fs, options ?? {});
}

export async function analyzeRRAsync(rrIntervals: number[], options?: HeartPyOptions): Promise<HeartPyResult> {
	const { NativeModules } = require('react-native');
	const Native: any = NativeModules?.HeartPyModule;
	if (!Native?.analyzeRRAsync) throw new Error('HeartPyModule.analyzeRRAsync not available');
	return Native.analyzeRRAsync(rrIntervals, options ?? {});
}

// Typed (bridge-optimized) variants - REMOVED DUPLICATE FUNCTIONS

export function analyzeSegmentwiseTyped(signal: number[] | Float64Array, fs: number, options?: HeartPyOptions): HeartPyResult {
	const { NativeModules, Platform } = require('react-native');
	const Native: any = NativeModules?.HeartPyModule;
	if (!Native?.analyzeSegmentwiseTyped) throw new Error('HeartPyModule.analyzeSegmentwiseTyped not available');
	const arr = (signal instanceof Float64Array ? Array.from(signal) : signal) as number[];
	return Native.analyzeSegmentwiseTyped(arr, fs, options ?? {});
}

export async function analyzeSegmentwiseAsyncTyped(signal: number[] | Float64Array, fs: number, options?: HeartPyOptions): Promise<HeartPyResult> {
	const { NativeModules } = require('react-native');
	const Native: any = NativeModules?.HeartPyModule;
	if (!Native?.analyzeSegmentwiseAsyncTyped) throw new Error('HeartPyModule.analyzeSegmentwiseAsyncTyped not available');
	const arr = (signal instanceof Float64Array ? Array.from(signal) : signal) as number[];
	return Native.analyzeSegmentwiseAsyncTyped(arr, fs, options ?? {});
}

export function analyzeRRTyped(rrIntervals: number[], options?: HeartPyOptions): HeartPyResult {
	const { NativeModules } = require('react-native');
	const Native: any = NativeModules?.HeartPyModule;
	if (!Native?.analyzeRRTyped) throw new Error('HeartPyModule.analyzeRRTyped not available');
	return Native.analyzeRRTyped(rrIntervals, options ?? {});
}

export async function analyzeRRAsyncTyped(rrIntervals: number[], options?: HeartPyOptions): Promise<HeartPyResult> {
	const { NativeModules } = require('react-native');
	const Native: any = NativeModules?.HeartPyModule;
	if (!Native?.analyzeRRAsyncTyped) throw new Error('HeartPyModule.analyzeRRAsyncTyped not available');
	return Native.analyzeRRAsyncTyped(rrIntervals, options ?? {});
}

// Optional JSI path (iOS installed via installJSI)
// ------------------------------
// Step 0: Risk mitigation flags & profiling (JS-only)
// ------------------------------

type RuntimeConfig = {
    jsiEnabled: boolean;
    zeroCopyEnabled: boolean;
    debug: boolean;
    maxSamplesPerPush: number;
};

const DEFAULT_CFG: RuntimeConfig = {
    jsiEnabled: true,
    zeroCopyEnabled: true,
    debug: false,
    maxSamplesPerPush: 5000,
};

let cfg: RuntimeConfig = { ...DEFAULT_CFG };
let sessionJSIDisabled = false; // permanent for this session once disabled

// Stats/profiling
const pushDurationsMs: number[] = [];
const pollDurationsMs: number[] = [];
let jsCalls = 0, nmCalls = 0, jsiCalls = 0;

function loadNativeConfig() {
    try {
        const { NativeModules } = require('react-native');
        const Native: any = NativeModules?.HeartPyModule;
        if (Native?.getConfig) {
            const m = Native.getConfig();
            cfg = {
                jsiEnabled: m?.jsiEnabled ?? cfg.jsiEnabled,
                zeroCopyEnabled: m?.zeroCopyEnabled ?? cfg.zeroCopyEnabled,
                debug: m?.debug ?? cfg.debug,
                maxSamplesPerPush: m?.maxSamplesPerPush ?? cfg.maxSamplesPerPush,
            };
        }
    } catch {}
}
loadNativeConfig();

function recordDuration(buf: number[], ms: number, cap = 100) {
    buf.push(ms);
    if (buf.length > cap) buf.shift();
}
function pctl(buf: number[], p: number): number {
    if (!buf.length) return 0;
    const a = buf.slice().sort((x, y) => x - y);
    const idx = Math.min(a.length - 1, Math.max(0, Math.floor((p / 100) * (a.length - 1))));
    return a[idx];
}

export function installJSI(): boolean {
    const { NativeModules } = require('react-native');
    const Native: any = NativeModules?.HeartPyModule;
    if (sessionJSIDisabled || !cfg.jsiEnabled || !Native?.installJSI) return false;
    try {
        const ok = !!Native.installJSI();
        if (!ok) sessionJSIDisabled = true; // rollback for session
        return ok;
    } catch (e) {
        // HEARTPY_E901: JSI unavailable
        if (cfg.debug) console.warn('HEARTPY_E901: JSI install failed', e);
        sessionJSIDisabled = true; // rollback for session
        return false;
    }
}

export function analyzeJSI(signal: number[] | Float64Array, fs: number, options?: HeartPyOptions): HeartPyResult {
	const g: any = global as any;
	if (g && typeof g.__HeartPyAnalyze === 'function') {
		return g.__HeartPyAnalyze(signal, fs, options ?? {});
	}
	throw new Error('JSI analyze not installed. Call installJSI() on iOS, or use NativeModules/async methods.');
}

// ------------------------------
// Realtime Streaming (NativeModules P0)
// ------------------------------

type HeartPyMetrics = HeartPyResult; // streaming returns same shape

export async function rtCreate(fs: number, options?: HeartPyOptions): Promise<number> {
    const { NativeModules } = require('react-native');
    const Native: any = NativeModules?.HeartPyModule;
    if (!Native?.rtCreate) throw new Error('HeartPyModule.rtCreate not available');
    jsCalls++;
    if (!(fs >= 1 && fs <= 10000)) {
        const err: any = new Error(`Invalid sample rate: ${fs}. Must be 1-10000 Hz.`);
        err.code = 'HEARTPY_E001';
        throw err;
    }
    return Native.rtCreate(fs, options ?? {});
}

export async function rtPush(handle: number, samples: Float32Array | number[], t0?: number): Promise<void> {
    const { NativeModules } = require('react-native');
    const Native: any = NativeModules?.HeartPyModule;
    if (!Native?.rtPush) throw new Error('HeartPyModule.rtPush not available');
    jsCalls++;
    const len = (samples instanceof Float32Array ? samples.length : Array.isArray(samples) ? samples.length : 0);
    if (!handle) { const e: any = new Error('Invalid or destroyed handle'); e.code = 'HEARTPY_E101'; throw e; }
    if (!len) { const e: any = new Error('Invalid data buffer: empty buffer'); e.code = 'HEARTPY_E102'; throw e; }
    if (len > cfg.maxSamplesPerPush) { const e: any = new Error(`Invalid data buffer: too large (max ${cfg.maxSamplesPerPush})`); e.code = 'HEARTPY_E102'; throw e; }
    const arr = (samples instanceof Float32Array ? Array.from(samples) : samples) as number[];
    const t1 = Date.now();
    const p = Native.rtPush(handle, arr, t0 ?? 0);
    nmCalls++;
    return p?.then?.(() => { recordDuration(pushDurationsMs, Date.now() - t1); }) ?? p;
}

export async function rtPoll(handle: number): Promise<HeartPyMetrics | null> {
    const { NativeModules } = require('react-native');
    const Native: any = NativeModules?.HeartPyModule;
    if (!Native?.rtPoll) throw new Error('HeartPyModule.rtPoll not available');
    jsCalls++;
    const t1 = Date.now();
    const p = Native.rtPoll(handle);
    nmCalls++;
    return p?.then?.((res: any) => {
        recordDuration(pollDurationsMs, Date.now() - t1);
        return (res ?? null);
    }) ?? p;
}

export async function rtDestroy(handle: number): Promise<void> {
    const { NativeModules } = require('react-native');
    const Native: any = NativeModules?.HeartPyModule;
    if (!Native?.rtDestroy) throw new Error('HeartPyModule.rtDestroy not available');
    return Native.rtDestroy(handle);
}

export async function rtSetWindow(handle: number, windowSeconds: number): Promise<void> {
    const { NativeModules } = require('react-native');
    const Native: any = NativeModules?.HeartPyModule;
    if (!Native?.rtSetWindow) throw new Error('HeartPyModule.rtSetWindow not available');
    if (!handle) {
        const e: any = new Error('Invalid or destroyed handle');
        e.code = 'HEARTPY_E101';
        throw e;
    }
    if (!(windowSeconds > 0)) {
        const e: any = new Error('Invalid windowSeconds: must be > 0');
        e.code = 'HEARTPY_E201';
        throw e;
    }
    return Native.rtSetWindow(handle, windowSeconds);
}

// Timestamped push (NativeModules path)
export async function rtPushTs(handle: number, samples: number[] | Float32Array, timestamps: number[] | Float64Array): Promise<void> {
    const { NativeModules } = require('react-native');
    const Native: any = NativeModules?.HeartPyModule;
    if (!Native?.rtPushTs) throw new Error('HeartPyModule.rtPushTs not available');
    jsCalls++;
    const xs = (samples instanceof Float32Array ? Array.from(samples) : samples) as number[];
    const ts = (timestamps instanceof Float64Array ? Array.from(timestamps) : timestamps) as number[];
    if (!handle) { const e: any = new Error('Invalid or destroyed handle'); e.code = 'HEARTPY_E101'; throw e; }
    if (!xs?.length || !ts?.length) { const e: any = new Error('Invalid buffers'); e.code = 'HEARTPY_E102'; throw e; }
    const t1 = Date.now();
    const p = Native.rtPushTs(handle, xs, ts);
    nmCalls++;
    return p?.then?.(() => { recordDuration(pushDurationsMs, Date.now() - t1); }) ?? p;
}

export class RealtimeAnalyzer {
    private handle: number = 0;
    private mode: 'nm' | 'jsi' = 'nm';
    private jsiId: number = 0; // 32-bit id when JSI is used
    private constructor(h: number) { this.handle = h; }

    static async create(fs: number, options?: HeartPyOptions): Promise<RealtimeAnalyzer> {
        const { Platform, NativeModules } = require('react-native');
        // Prefer JSI only on Android when enabled and successfully installed
        let useJSI = false;
        if (Platform.OS === 'android' && cfg.jsiEnabled && !sessionJSIDisabled) {
            try {
                const ok = !!NativeModules?.HeartPyModule?.installJSI?.();
                const g: any = global as any;
                if (ok && typeof g.__hpRtCreate === 'function' && typeof g.__hpRtPush === 'function' && typeof g.__hpRtPoll === 'function' && typeof g.__hpRtDestroy === 'function') {
                    useJSI = true;
                }
            } catch { /* ignore */ }
        }

        if (useJSI) {
            try {
                const g: any = global as any;
                // JSI path does native validation; TS still guards fs bounds early
                if (!(fs >= 1 && fs <= 10000)) {
                    const err: any = new Error(`Invalid sample rate: ${fs}. Must be 1-10000 Hz.`);
                    err.code = 'HEARTPY_E001';
                    throw err;
                }
                const id = g.__hpRtCreate(fs, options ?? {});
                const inst = new RealtimeAnalyzer(0);
                inst.mode = 'jsi';
                inst.jsiId = id | 0;
                if (options?.windowSeconds != null) {
                    try {
                        await inst.setWindow(options.windowSeconds);
                    } catch (e) {
                        if (cfg.debug) console.warn('HeartPy: __hpRtSetWindow failed', e);
                    }
                }
                if (cfg.debug) console.log('HeartPy: using JSI path');
                return inst;
            } catch (e) {
                // fallthrough to NM path on any error
                if (cfg.debug) console.warn('HeartPy: JSI path failed, falling back to NativeModules', e);
                sessionJSIDisabled = true;
            }
        }

        const h = await rtCreate(fs, options);
        const inst = new RealtimeAnalyzer(h);
        if (options?.windowSeconds != null) {
            try {
                await inst.setWindow(options.windowSeconds);
            } catch (e) {
                if (cfg.debug) console.warn('HeartPy: rtSetWindow failed', e);
            }
        }
        if (cfg.debug) console.log('HeartPy: using NativeModules path');
        return inst;
    }

    async setWindow(windowSeconds: number): Promise<void> {
        if (!(windowSeconds > 0)) {
            const e: any = new Error('Invalid windowSeconds: must be > 0');
            e.code = 'HEARTPY_E201';
            throw e;
        }
        if (this.mode === 'jsi') {
            if (!this.jsiId) throw new Error('RealtimeAnalyzer destroyed');
            const g: any = global as any;
            if (typeof g.__hpRtSetWindow !== 'function') {
                throw new Error('HeartPy JSI setWindow not available');
            }
            g.__hpRtSetWindow(this.jsiId, windowSeconds);
            return;
        }
        if (!this.handle) throw new Error('RealtimeAnalyzer destroyed');
        return rtSetWindow(this.handle, windowSeconds);
    }

    async push(samples: Float32Array | number[], t0?: number): Promise<void> {
        if (this.mode === 'jsi') {
            if (!this.jsiId) throw new Error('RealtimeAnalyzer destroyed');
            const g: any = global as any;
            jsiCalls++;
            // Prefer Float32Array for zero-copy later
            const buf = (samples instanceof Float32Array ? samples : new Float32Array(samples as number[]));
            const t1 = Date.now();
            g.__hpRtPush(this.jsiId, buf, t0 ?? 0);
            recordDuration(pushDurationsMs, Date.now() - t1);
            return;
        } else {
            if (!this.handle) throw new Error('RealtimeAnalyzer destroyed');
            return rtPush(this.handle, samples, t0 ?? Date.now() / 1000);
        }
    }

    async poll(): Promise<HeartPyMetrics | null> {
        if (this.mode === 'jsi') {
            if (!this.jsiId) throw new Error('RealtimeAnalyzer destroyed');
            const g: any = global as any;
            jsiCalls++;
            const t1 = Date.now();
            const res = g.__hpRtPoll(this.jsiId);
            recordDuration(pollDurationsMs, Date.now() - t1);
            return res ?? null;
        } else {
            if (!this.handle) throw new Error('RealtimeAnalyzer destroyed');
            return rtPoll(this.handle);
        }
    }

    async destroy(): Promise<void> {
        if (this.mode === 'jsi') {
            if (!this.jsiId) return;
            const id = this.jsiId; this.jsiId = 0;
            try { (global as any).__hpRtDestroy(id); } catch {}
        } else {
            if (!this.handle) return; // idempotent
            const h = this.handle; this.handle = 0;
            try { await rtDestroy(h); } catch {}
        }
    }

    async pushWithTimestamps(samples: Float32Array | number[], timestamps: Float64Array | number[]): Promise<void> {
        if (this.mode === 'jsi') {
            // Fallback to non-TS push for JSI
            return this.push(samples);
        }
        if (!this.handle) throw new Error('RealtimeAnalyzer destroyed');
        return rtPushTs(this.handle, (samples as any), (timestamps as any));
    }

    // Allow dev-time override of flags
    static setConfig(next: Partial<RuntimeConfig>) {
        cfg = { ...cfg, ...next } as RuntimeConfig;
        try {
            const { NativeModules } = require('react-native');
            const Native: any = NativeModules?.HeartPyModule;
            if (Native?.setConfig) Native.setConfig(next);
        } catch {}
    }
}

// Debugger utility
export const HeartPyDebugger = {
    getStats() {
        return {
            jsCalls,
            jsiCalls,
            nmCalls,
            pushMsP50: pctl(pushDurationsMs, 50),
            pushMsP95: pctl(pushDurationsMs, 95),
            pollMsP50: pctl(pollDurationsMs, 50),
            pollMsP95: pctl(pollDurationsMs, 95),
        };
    },
};
 
