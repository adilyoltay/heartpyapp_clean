import {Platform} from 'react-native';

import {getNativeModule} from './NativeHeartPy';
import type {HeartPyModule} from './NativeHeartPy';
import {
	HeartPyMetrics,
	HeartPyOptions,
	HeartPyResult,
	NumericArray,
	RealtimeHandle,
	RuntimeConfig,
	RuntimeConfigPatch,
} from './types/heartpy';

export type {
	HeartPyMetrics,
	HeartPyOptions,
	HeartPyResult,
	NumericArray,
	RealtimeHandle,
	RuntimeConfig,
	RuntimeConfigPatch,
} from './types/heartpy';

export type QualityInfo = HeartPyResult['quality'];

type NumericInput = number[] | Float32Array | Float64Array;

type ModuleFunction<T extends keyof HeartPyModule> = HeartPyModule[T] extends (...args: infer P) => infer R
	? (...args: P) => R
	: never;

const DEFAULT_CFG: RuntimeConfig = {
	jsiEnabled: true,
	zeroCopyEnabled: true,
	debug: false,
	maxSamplesPerPush: 5000,
};

let cfg: RuntimeConfig = {...DEFAULT_CFG};
let sessionJSIDisabled = false;

const pushDurationsMs: number[] = [];
const pollDurationsMs: number[] = [];
let jsCalls = 0;
let nmCalls = 0;
let jsiCalls = 0;

function ensureOptions(options?: HeartPyOptions | null): HeartPyOptions {
	return options ?? {};
}

function asNumberArray(input: NumericInput): number[] {
	if (Array.isArray(input)) {
		return input as number[];
	}
	return Array.from(input);
}

function toPromise<T>(value: Promise<T> | T): Promise<T> {
	return Promise.resolve(value);
}

function recordDuration(buf: number[], ms: number, cap = 100) {
	buf.push(ms);
	if (buf.length > cap) {
		buf.shift();
	}
}

function pctl(buf: number[], p: number): number {
	if (!buf.length) {
		return 0;
	}
	const a = buf.slice().sort((x, y) => x - y);
	const idx = Math.min(a.length - 1, Math.max(0, Math.floor((p / 100) * (a.length - 1))));
	return a[idx];
}

function requireMethod<T extends keyof HeartPyModule>(module: HeartPyModule, method: T): ModuleFunction<T> {
	const fn = module[method];
	if (typeof fn !== 'function') {
		throw new Error(`HeartPyModule.${String(method)} not available`);
	}
	return fn as ModuleFunction<T>;
}

function loadNativeConfig() {
	try {
		const native = getNativeModule();
		if (typeof native.getConfig === 'function') {
			const next = native.getConfig() as Partial<RuntimeConfig> | undefined;
			cfg = {
				jsiEnabled: next?.jsiEnabled ?? cfg.jsiEnabled,
				zeroCopyEnabled: next?.zeroCopyEnabled ?? cfg.zeroCopyEnabled,
				debug: next?.debug ?? cfg.debug,
				maxSamplesPerPush: next?.maxSamplesPerPush ?? cfg.maxSamplesPerPush,
			};
		}
	} catch {
		// noop: native module not yet ready
	}
}

loadNativeConfig();

export function analyze(signal: NumericInput, fs: number, options?: HeartPyOptions | null): HeartPyResult {
	const native = getNativeModule();
	const analyzeFn = requireMethod(native, 'analyze');
	const arr = asNumberArray(signal);
	return analyzeFn(arr, fs, ensureOptions(options)) as HeartPyResult;
}

export function analyzeSegmentwise(signal: NumericInput, fs: number, options?: HeartPyOptions | null): HeartPyResult {
	const native = getNativeModule();
	const analyzeFn = requireMethod(native, 'analyzeSegmentwise');
	const arr = asNumberArray(signal);
	return analyzeFn(arr, fs, ensureOptions(options)) as HeartPyResult;
}

export function analyzeRR(rrIntervals: number[], options?: HeartPyOptions | null): HeartPyResult {
	const native = getNativeModule();
	const analyzeFn = requireMethod(native, 'analyzeRR');
	return analyzeFn(rrIntervals, ensureOptions(options)) as HeartPyResult;
}

export async function analyzeAsync(signal: NumericInput, fs: number, options?: HeartPyOptions | null): Promise<HeartPyResult> {
	const native = getNativeModule();
	const analyzeFn = requireMethod(native, 'analyzeAsync');
	const arr = asNumberArray(signal);
	return (await analyzeFn(arr, fs, ensureOptions(options))) as HeartPyResult;
}

export function analyzeTyped(signal: NumericInput, fs: number, options?: HeartPyOptions | null): HeartPyResult {
	const native = getNativeModule();
	const analyzeFn = requireMethod(native, 'analyzeTyped');
	const arr = asNumberArray(signal);
	return analyzeFn(arr, fs, ensureOptions(options)) as HeartPyResult;
}

export async function analyzeAsyncTyped(signal: NumericInput, fs: number, options?: HeartPyOptions | null): Promise<HeartPyResult> {
	const native = getNativeModule();
	const analyzeFn = requireMethod(native, 'analyzeAsyncTyped');
	const arr = asNumberArray(signal);
	return (await analyzeFn(arr, fs, ensureOptions(options))) as HeartPyResult;
}

export function analyzeSegmentwiseTyped(signal: NumericInput, fs: number, options?: HeartPyOptions | null): HeartPyResult {
	const native = getNativeModule();
	const analyzeFn = requireMethod(native, 'analyzeSegmentwiseTyped');
	const arr = asNumberArray(signal);
	return analyzeFn(arr, fs, ensureOptions(options)) as HeartPyResult;
}

export async function analyzeSegmentwiseAsync(signal: NumericInput, fs: number, options?: HeartPyOptions | null): Promise<HeartPyResult> {
	const native = getNativeModule();
	const analyzeFn = requireMethod(native, 'analyzeSegmentwiseAsync');
	const arr = asNumberArray(signal);
	return (await analyzeFn(arr, fs, ensureOptions(options))) as HeartPyResult;
}

export async function analyzeSegmentwiseAsyncTyped(signal: NumericInput, fs: number, options?: HeartPyOptions | null): Promise<HeartPyResult> {
	const native = getNativeModule();
	const analyzeFn = requireMethod(native, 'analyzeSegmentwiseAsyncTyped');
	const arr = asNumberArray(signal);
	return (await analyzeFn(arr, fs, ensureOptions(options))) as HeartPyResult;
}

export function analyzeRRTyped(rrIntervals: number[], options?: HeartPyOptions | null): HeartPyResult {
	const native = getNativeModule();
	const analyzeFn = requireMethod(native, 'analyzeRRTyped');
	return analyzeFn(rrIntervals, ensureOptions(options)) as HeartPyResult;
}

export async function analyzeRRAsync(rrIntervals: number[], options?: HeartPyOptions | null): Promise<HeartPyResult> {
	const native = getNativeModule();
	const analyzeFn = requireMethod(native, 'analyzeRRAsync');
	return (await analyzeFn(rrIntervals, ensureOptions(options))) as HeartPyResult;
}

export async function analyzeRRAsyncTyped(rrIntervals: number[], options?: HeartPyOptions | null): Promise<HeartPyResult> {
	const native = getNativeModule();
	const analyzeFn = requireMethod(native, 'analyzeRRAsyncTyped');
	return (await analyzeFn(rrIntervals, ensureOptions(options))) as HeartPyResult;
}

export function interpolateClipping(signal: number[], fs: number, threshold = 1020): NumericArray {
	const native = getNativeModule();
	const fn = requireMethod(native, 'interpolateClipping');
	return fn(signal, fs, threshold);
}

export function hampelFilter(signal: number[], windowSize = 6, threshold = 3.0): NumericArray {
	const native = getNativeModule();
	const fn = requireMethod(native, 'hampelFilter');
	return fn(signal, windowSize, threshold);
}

export function scaleData(signal: number[], newMin = 0, newMax = 1024): NumericArray {
	const native = getNativeModule();
	const fn = requireMethod(native, 'scaleData');
	return fn(signal, newMin, newMax);
}

export function installJSI(): boolean {
	if (sessionJSIDisabled || !cfg.jsiEnabled) {
		return false;
	}
	try {
		const native = getNativeModule();
		if (typeof native.installJSI !== 'function') {
			return false;
		}
		const ok = !!native.installJSI();
		if (!ok) {
			sessionJSIDisabled = true;
		}
		return ok;
	} catch (e) {
		if (cfg.debug) {
			console.warn('HEARTPY_E901: JSI install failed', e);
		}
		sessionJSIDisabled = true;
		return false;
	}
}

export function analyzeJSI(signal: NumericInput, fs: number, options?: HeartPyOptions | null): HeartPyResult {
	const g: any = global;
	if (g && typeof g.__HeartPyAnalyze === 'function') {
		return g.__HeartPyAnalyze(signal, fs, ensureOptions(options));
	}
	throw new Error('JSI analyze not installed. Call installJSI() on iOS, or use NativeModules/async methods.');
}

function ensureHandle(handle: RealtimeHandle): asserts handle {
	if (!handle) {
		const e: any = new Error('Invalid or destroyed handle');
		e.code = 'HEARTPY_E101';
		throw e;
	}
}

export async function rtCreate(fs: number, options?: HeartPyOptions | null): Promise<RealtimeHandle> {
	const native = getNativeModule();
	const fn = requireMethod(native, 'rtCreate');
	jsCalls++;
	if (!(fs >= 1 && fs <= 10000)) {
		const err: any = new Error(`Invalid sample rate: ${fs}. Must be 1-10000 Hz.`);
		err.code = 'HEARTPY_E001';
		throw err;
	}
	const handle = await toPromise(fn(fs, ensureOptions(options)));
	return handle as number;
}

export async function rtPush(handle: RealtimeHandle, samples: NumericInput, t0?: number): Promise<void> {
	const native = getNativeModule();
	const fn = requireMethod(native, 'rtPush');
	jsCalls++;
	ensureHandle(handle);
	const arr = asNumberArray(samples);
	const len = arr.length;
	if (!len) {
		const e: any = new Error('Invalid data buffer: empty buffer');
		e.code = 'HEARTPY_E102';
		throw e;
	}
	if (len > cfg.maxSamplesPerPush) {
		const e: any = new Error(`Invalid data buffer: too large (max ${cfg.maxSamplesPerPush})`);
		e.code = 'HEARTPY_E102';
		throw e;
	}
	const t1 = Date.now();
	const res = fn(handle, arr, t0 ?? 0);
	nmCalls++;
	return toPromise(res).then(() => {
		recordDuration(pushDurationsMs, Date.now() - t1);
	});
}

export async function rtPushTs(handle: RealtimeHandle, samples: NumericInput, timestamps: NumericInput): Promise<void> {
	const native = getNativeModule();
	const fn = requireMethod(native, 'rtPushTs');
	jsCalls++;
	ensureHandle(handle);
	const xs = asNumberArray(samples);
	const ts = asNumberArray(timestamps);
	if (!xs.length || !ts.length) {
		const e: any = new Error('Invalid buffers');
		e.code = 'HEARTPY_E102';
		throw e;
	}
	const t1 = Date.now();
	const res = fn(handle, xs, ts);
	nmCalls++;
	return toPromise(res).then(() => {
		recordDuration(pushDurationsMs, Date.now() - t1);
	});
}

export async function rtPoll(handle: RealtimeHandle): Promise<HeartPyMetrics | null> {
	const native = getNativeModule();
	const fn = requireMethod(native, 'rtPoll');
	jsCalls++;
	ensureHandle(handle);
	const t1 = Date.now();
	const result = await toPromise(fn(handle));
	nmCalls++;
	recordDuration(pollDurationsMs, Date.now() - t1);
	return (result as HeartPyMetrics | null) ?? null;
}

export async function rtSetWindow(handle: RealtimeHandle, windowSeconds: number): Promise<void> {
	const native = getNativeModule();
	const fn = requireMethod(native, 'rtSetWindow');
	ensureHandle(handle);
	if (!(windowSeconds > 0)) {
		const e: any = new Error('Invalid windowSeconds: must be > 0');
		e.code = 'HEARTPY_E201';
		throw e;
	}
	await toPromise(fn(handle, windowSeconds));
}

export async function rtDestroy(handle: RealtimeHandle): Promise<void> {
	const native = getNativeModule();
	const fn = requireMethod(native, 'rtDestroy');
	await toPromise(fn(handle));
}

const HeartPyConfig = {
	get(): RuntimeConfig {
		return {...cfg};
	},
	set(patch: RuntimeConfigPatch) {
		cfg = {...cfg, ...patch};
		try {
			const native = getNativeModule();
			if (typeof native.setConfig === 'function') {
				native.setConfig(patch);
			}
		} catch {
			// ignore when native module unavailable (tests)
		}
	},
};

export class RealtimeAnalyzer {
	private handle: RealtimeHandle = 0;
	private mode: 'nm' | 'jsi' = 'nm';
	private jsiId = 0;

	private constructor(h: RealtimeHandle) {
		this.handle = h;
	}

	static async create(fs: number, options?: HeartPyOptions | null): Promise<RealtimeAnalyzer> {
		let useJSI = false;
		if (Platform.OS === 'android' && cfg.jsiEnabled && !sessionJSIDisabled) {
			try {
				const native = getNativeModule();
				const ok = typeof native.installJSI === 'function' ? !!native.installJSI() : false;
				const g: any = global;
				if (
					ok &&
					typeof g.__hpRtCreate === 'function' &&
					typeof g.__hpRtPush === 'function' &&
					typeof g.__hpRtPoll === 'function' &&
					typeof g.__hpRtDestroy === 'function'
				) {
					useJSI = true;
				}
			} catch (e) {
				if (cfg.debug) {
					console.warn('HeartPy: JSI path failed, falling back to NativeModules', e);
				}
				sessionJSIDisabled = true;
			}
		}

		if (useJSI) {
			try {
				const g: any = global;
				if (!(fs >= 1 && fs <= 10000)) {
					const err: any = new Error(`Invalid sample rate: ${fs}. Must be 1-10000 Hz.`);
					err.code = 'HEARTPY_E001';
					throw err;
				}
				const id = g.__hpRtCreate(fs, ensureOptions(options));
				const inst = new RealtimeAnalyzer(0);
				inst.mode = 'jsi';
				inst.jsiId = id | 0;
				if (options?.windowSeconds != null) {
					try {
						await inst.setWindow(options.windowSeconds);
					} catch (e) {
						if (cfg.debug) {
							console.warn('HeartPy: __hpRtSetWindow failed', e);
						}
					}
				}
				if (cfg.debug) {
					console.log('HeartPy: using JSI path');
				}
				return inst;
			} catch (e) {
				sessionJSIDisabled = true;
				if (cfg.debug) {
					console.warn('HeartPy: JSI path failed, falling back to NativeModules', e);
				}
			}
		}

		const handle = await rtCreate(fs, options);
		const inst = new RealtimeAnalyzer(handle);
		if (options?.windowSeconds != null) {
			try {
				await inst.setWindow(options.windowSeconds);
			} catch (e) {
				if (cfg.debug) {
					console.warn('HeartPy: rtSetWindow failed', e);
				}
			}
		}
		if (cfg.debug) {
			console.log('HeartPy: using NativeModules path');
		}
		return inst;
	}

	async setWindow(windowSeconds: number): Promise<void> {
		if (!(windowSeconds > 0)) {
			const e: any = new Error('Invalid windowSeconds: must be > 0');
			e.code = 'HEARTPY_E201';
			throw e;
		}
		if (this.mode === 'jsi') {
			if (!this.jsiId) {
				throw new Error('RealtimeAnalyzer destroyed');
			}
			const g: any = global;
			if (typeof g.__hpRtSetWindow !== 'function') {
				throw new Error('HeartPy JSI setWindow not available');
			}
			g.__hpRtSetWindow(this.jsiId, windowSeconds);
			return;
		}
		ensureHandle(this.handle);
		return rtSetWindow(this.handle, windowSeconds);
	}

	async push(samples: NumericInput, t0?: number): Promise<void> {
		if (this.mode === 'jsi') {
			if (!this.jsiId) {
				throw new Error('RealtimeAnalyzer destroyed');
			}
			const g: any = global;
			jsiCalls++;
			const buf = samples instanceof Float32Array ? samples : new Float32Array(asNumberArray(samples));
			const t1 = Date.now();
			g.__hpRtPush(this.jsiId, buf, t0 ?? 0);
			recordDuration(pushDurationsMs, Date.now() - t1);
			return;
		}
		ensureHandle(this.handle);
		return rtPush(this.handle, samples, t0 ?? Date.now() / 1000);
	}

	async pushWithTimestamps(samples: NumericInput, timestamps: NumericInput): Promise<void> {
		if (this.mode === 'jsi') {
			if (!this.jsiId) {
				throw new Error('RealtimeAnalyzer destroyed');
			}
			const g: any = global;
			if (typeof g.__hpRtPushTs !== 'function') {
				throw new Error('HeartPy JSI pushWithTimestamps not available');
			}
			jsiCalls++;
			const samplesBuf = samples instanceof Float32Array ? samples : new Float32Array(asNumberArray(samples));
			const timestampsBuf = timestamps instanceof Float64Array ? timestamps : new Float64Array(asNumberArray(timestamps));
			if (!samplesBuf.length || samplesBuf.length !== timestampsBuf.length) {
				const err: any = new Error('Invalid buffers');
				err.code = 'HEARTPY_E102';
				throw err;
			}
			const t1 = Date.now();
			g.__hpRtPushTs(this.jsiId, samplesBuf, timestampsBuf);
			recordDuration(pushDurationsMs, Date.now() - t1);
			return;
		}
		ensureHandle(this.handle);
		return rtPushTs(this.handle, samples, timestamps);
	}

	async poll(): Promise<HeartPyMetrics | null> {
		if (this.mode === 'jsi') {
			if (!this.jsiId) {
				throw new Error('RealtimeAnalyzer destroyed');
			}
			const g: any = global;
			jsiCalls++;
			const t1 = Date.now();
			const res = g.__hpRtPoll(this.jsiId);
			recordDuration(pollDurationsMs, Date.now() - t1);
			return res ?? null;
		}
		ensureHandle(this.handle);
		return rtPoll(this.handle);
	}

	async destroy(): Promise<void> {
		if (this.mode === 'jsi') {
			if (!this.jsiId) {
				return;
			}
			const id = this.jsiId;
			this.jsiId = 0;
			try {
				(global as any).__hpRtDestroy(id);
			} catch {
				// ignore
			}
			return;
		}
		if (!this.handle) {
			return;
		}
		const h = this.handle;
		this.handle = 0;
		try {
			await rtDestroy(h);
		} catch {
			// ignore errors during destroy
		}
	}

	static setConfig(next: RuntimeConfigPatch) {
		cfg = {...cfg, ...next};
		try {
			const native = getNativeModule();
			if (typeof native.setConfig === 'function') {
				native.setConfig(next);
			}
		} catch {
			// ignore in tests
		}
	}
}

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

export const HeartPyRuntime = HeartPyConfig;
